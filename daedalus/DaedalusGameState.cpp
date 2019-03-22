//
// Created by andre on 24.05.16.
//

#include "DaedalusGameState.h"
#include "DaedalusDialogManager.h"
#include "DaedalusVM.h"
#include <utils/logger.h>

using namespace ZenLoad;
using namespace Daedalus;
using namespace GameState;

DaedalusGameState::DaedalusGameState(Daedalus::DaedalusVM& vm)
    : m_VM(vm)
{
    // Register external lib-functions
}

Daedalus::GEngineClasses::Instance* DaedalusGameState::getByClass(ZMemory::BigHandle h, EInstanceClass instClass)
{
    if (!h.isValid()) return nullptr;

    switch (instClass)
    {
        case IC_Npc:
            return &getNpc(ZMemory::handleCast<NpcHandle>(h));

        case IC_Item:
            return &getItem(ZMemory::handleCast<ItemHandle>(h));

        case IC_Mission:
            return &getMission(ZMemory::handleCast<MissionHandle>(h));

        case IC_Info:
            return &getInfo(ZMemory::handleCast<InfoHandle>(h));

        case IC_ItemReact:
            return &getItemReact(ZMemory::handleCast<ItemReactHandle>(h));

        case IC_Focus:
            return &getFocus(ZMemory::handleCast<FocusHandle>(h));

        case IC_Menu:
            return &getMenu(ZMemory::handleCast<MenuHandle>(h));

        case IC_MenuItem:
            return &getMenuItem(ZMemory::handleCast<MenuItemHandle>(h));

        case IC_Sfx:
            return &getSfx(ZMemory::handleCast<SfxHandle>(h));

        case IC_Pfx:
            return &getPfx(ZMemory::handleCast<PfxHandle>(h));

        case IC_MusicTheme:
            return &getMusicTheme(ZMemory::handleCast<MusicThemeHandle>(h));

        case IC_GilValues:
            return &getGilValues(ZMemory::handleCast<GilValuesHandle>(h));

        default:
            return nullptr;
    }
}

template <typename C_Class>
CHandle<C_Class> DaedalusGameState::create()
{
    CHandle<C_Class> h = m_RegisteredObjects.get<C_Class>().createObject();
    // important! overwrite uninitialized memory with initialized C_Class
    get<C_Class>(h) = C_Class();

    if (m_OnInstanceCreated)
        m_OnInstanceCreated(ZMemory::toBigHandle(h), enumFromClass<C_Class>());

    return h;
}

NpcHandle DaedalusGameState::createNPC()
{
    return create<GEngineClasses::C_Npc>();
}

ItemHandle DaedalusGameState::createItem()
{
    return create<GEngineClasses::C_Item>();
}

ItemReactHandle DaedalusGameState::createItemReact()
{
    return create<GEngineClasses::C_ItemReact>();
}

MissionHandle DaedalusGameState::createMission()
{
    return create<GEngineClasses::C_Mission>();
}

InfoHandle DaedalusGameState::createInfo()
{
    return create<GEngineClasses::C_Info>();
}

FocusHandle DaedalusGameState::createFocus()
{
    return create<GEngineClasses::C_Focus>();
}

SfxHandle DaedalusGameState::createSfx()
{
    return create<GEngineClasses::C_SFX>();
}

PfxHandle DaedalusGameState::createPfx()
{
    return create<GEngineClasses::C_ParticleFX>();
}

FocusHandle DaedalusGameState::createMenu()
{
    return create<GEngineClasses::C_Menu>();
}

FocusHandle DaedalusGameState::createMenuItem()
{
    return create<GEngineClasses::C_Menu_Item>();
}

MusicThemeHandle DaedalusGameState::createMusicTheme()
{
  return create<GEngineClasses::C_MusicTheme>();
}

GilValuesHandle DaedalusGameState::createGilValues()
{
  return create<GEngineClasses::C_GilValues>();
}

ItemHandle DaedalusGameState::createInventoryItem(size_t itemSymbol, NpcHandle npc, unsigned int count)
{
    auto items = m_NpcInventories[npc];

    // Try to find an item of this type
    for (ItemHandle h : items)
    {
        GEngineClasses::C_Item& item = getItem(h);

        // Just add to the count here
        if (item.instanceSymbol == itemSymbol)
        {
            item.amount += count;
            return h;
        }
    }

    static int s_Tmp = 0;
    s_Tmp++;

    // Get memory for the item
    ItemHandle h = createItem();
    GEngineClasses::C_Item& item = getItem(h);

    item.amount = count;

    // Run the script-constructor
    m_VM.initializeInstance(ZMemory::toBigHandle(h), static_cast<size_t>(itemSymbol), IC_Item);

    // Put inside its inventory
    addItemToInventory(h, npc);

    return h;
}

ItemHandle DaedalusGameState::addItemToInventory(ItemHandle item, NpcHandle npc)
{
    auto items = m_NpcInventories[npc];

    // Try to find an item of this type
    for (ItemHandle h : items)
    {
        GEngineClasses::C_Item& i = getItem(h);

        // Just add to the count here
        if (i.instanceSymbol == getItem(item).instanceSymbol)
        {
            i.amount++;
            return h;
        }
    }

    m_NpcInventories[npc].push_back(item);

    if (m_GameExternals.createinvitem)
        m_GameExternals.createinvitem(item, npc);

    return item;
}

bool DaedalusGameState::removeInventoryItem(size_t itemSymbol, NpcHandle npc, unsigned int count)
{
    for (auto it = m_NpcInventories[npc].begin(); it != m_NpcInventories[npc].end(); it++)
    {
        Daedalus::GEngineClasses::C_Item& item = getItem((*it));

        if (item.instanceSymbol == itemSymbol)
        {
            item.amount -= std::min(item.amount, count);  // Handle overflow;

            // Remove if count reached 0
            if (item.amount == 0)
            {
                removeItem(*it);

                m_NpcInventories[npc].erase(it);
            }

            return true;
        }
    }

    return false;
}

NpcHandle DaedalusGameState::insertNPC(size_t instance, const std::string& waypoint)
{
    NpcHandle npc = createNPC();

    GEngineClasses::C_Npc& npcData = getNpc(npc);
    npcData.wp = waypoint;

    // Setup basic class linkage. This is important here, as otherwise the wld_insertnpc-callback won't be
    // able to work properly
    npcData.instanceSymbol = instance;

    PARSymbol& s = m_VM.getDATFile().getSymbolByIndex(instance);
    s.instanceDataHandle = ZMemory::toBigHandle(npc);
    s.instanceDataClass = IC_Npc;

    if (m_GameExternals.wld_insertnpc)
        m_GameExternals.wld_insertnpc(npc, waypoint);

    m_VM.initializeInstance(ZMemory::toBigHandle(npc), instance, IC_Npc);

    // Init complete, notify the engine
    if (m_GameExternals.post_wld_insertnpc)
        m_GameExternals.post_wld_insertnpc(npc);

    return npc;
}

NpcHandle DaedalusGameState::insertNPC(const std::string& instance, const std::string& waypoint)
{
    return insertNPC(m_VM.getDATFile().getSymbolIndexByName(instance), waypoint);
}

ItemHandle DaedalusGameState::insertItem(size_t instance)
{
    // Get memory for the item
    ItemHandle h = createItem();
    GEngineClasses::C_Item& item = getItem(h);

    // Run the script-constructor
    m_VM.initializeInstance(ZMemory::toBigHandle(h), static_cast<size_t>(instance), IC_Item);

    return h;
}

ItemHandle DaedalusGameState::insertItem(const std::string& instance)
{
    return insertItem(m_VM.getDATFile().getSymbolIndexByName(instance));
}

SfxHandle DaedalusGameState::insertSFX(size_t instance)
{
    // Get memory for the item
    SfxHandle h = createSfx();
    GEngineClasses::C_SFX& sfx = getSfx(h);

    // Run the script-constructor
    m_VM.initializeInstance(ZMemory::toBigHandle(h), static_cast<size_t>(instance), IC_Sfx);

    return h;
}

SfxHandle DaedalusGameState::insertSFX(const std::string& instance)
{
    return insertSFX(m_VM.getDATFile().getSymbolIndexByName(instance));
}

MusicThemeHandle DaedalusGameState::insertMusicTheme(size_t instance)
{
    // Get memory for the item
    MusicThemeHandle h = createMusicTheme();
    GEngineClasses::C_MusicTheme& mt = getMusicTheme(h);

    // Run the script-constructor
    m_VM.initializeInstance(ZMemory::toBigHandle(h), static_cast<size_t>(instance), IC_MusicTheme);

    return h;
}

MusicThemeHandle DaedalusGameState::insertMusicTheme(const std::string& instance)
{
    return insertMusicTheme(m_VM.getDATFile().getSymbolIndexByName(instance));
}

void DaedalusGameState::removeItem(ItemHandle item)
{
    if (m_OnInstanceRemoved)
        m_OnInstanceRemoved(ZMemory::toBigHandle(item), IC_Item);

    m_RegisteredObjects.items.removeObject(item);
}

void DaedalusGameState::removeMenu(MenuHandle menu)
{
    if (m_OnInstanceRemoved)
        m_OnInstanceRemoved(ZMemory::toBigHandle(menu), IC_Menu);

    m_RegisteredObjects.menus.removeObject(menu);
}

void DaedalusGameState::removeMenuItem(MenuItemHandle menuItem)
{
    if (m_OnInstanceRemoved)
        m_OnInstanceRemoved(ZMemory::toBigHandle(menuItem), IC_MenuItem);

    m_RegisteredObjects.menuItems.removeObject(menuItem);
}

void DaedalusGameState::removeNPC(NpcHandle npc)
{
    if (m_GameExternals.wld_removenpc)
        m_GameExternals.wld_removenpc(npc);

    if (m_OnInstanceRemoved)
        m_OnInstanceRemoved(ZMemory::toBigHandle(npc), IC_Npc);

    m_RegisteredObjects.NPCs.removeObject(npc);
}
