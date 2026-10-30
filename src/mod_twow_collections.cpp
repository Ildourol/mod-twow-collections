#include "CollectionsCommon.h"
#include "CollectionsMgr.h"

extern void AddCollectionsScriptHooks();

void Addmod_twow_collectionsScripts()
{
    sCollectionsMgr.Initialize();
    AddCollectionsScriptHooks();
}
