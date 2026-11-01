// Android billing is unavailable. Complete catalogue requests with zero items;
// never report a purchase or grant currency through this adapter.
#include "gangstar_iap_offline.h"
#include "utils/logger.h"
#include "utils/dialog.h"
#include <so_util/so_util.h>
#include <kubridge.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
extern so_module so_mod;
static void (*catalogue_callback)(const char *,int);
static atomic_int catalogue_pending;
static int offline_billing_state(void) {return 0;} // APK GetState default: no transaction.
static int offline_total_items(void) {return 0;}
static void offline_catalogue_request(const char *category) {
    (void)category;
    atomic_store(&catalogue_pending,1);
}
void gangstar_iap_offline_poll(void) {
    if(!atomic_load_explicit(&catalogue_pending,memory_order_relaxed))return;
    if(!atomic_exchange(&catalogue_pending,0))return;
    // ItemManager holds its mutex while requesting a catalogue. Deliver on
    // the next main-loop poll, after that request has returned and unlocked.
    catalogue_callback("offline",0);
    l_info("RIO_IAP_OFFLINE catalogue_complete items=0");
}
void gangstar_iap_offline_install(void) {
    static const struct {const char *name;uint32_t expected[2];uintptr_t replacement;} hooks[]={
        {"_ZN12InAppBilling8getStateEv",{0xe92d40f0,0xe59f40c0},(uintptr_t)offline_billing_state},
        {"_ZN12InAppBilling13getTotalItemsEv",{0xe59f3014,0xe79f3003},(uintptr_t)offline_total_items},
        {"_ZN12InAppBilling11getItemListEPKc",{0xe92d47f0,0xe59f413c},(uintptr_t)offline_catalogue_request}
    };
    uintptr_t targets[3];
    catalogue_callback=(void *)so_symbol(&so_mod,"InAppBilling_GetItemListCB");
    if(!catalogue_callback)fatal_error("Gangstar offline billing: missing catalogue callback");
    for(unsigned i=0;i<3;++i) {
        targets[i]=so_symbol(&so_mod,hooks[i].name);
        if(!targets[i] || memcmp((void *)targets[i],hooks[i].expected,8))
            fatal_error("Gangstar offline billing: unsupported %s",hooks[i].name);
    }
    for(unsigned i=0;i<3;++i) {
        hook_addr(targets[i],hooks[i].replacement);
        kuKernelFlushCaches((void *)targets[i],8);
    }
    l_info("Gangstar offline billing: empty catalogue; no transaction pending");
}
