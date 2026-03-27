// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <trace/hooks/mm.h>

#include "soc/xring/xring_mem_adapter.h"

#include "xring_mem_adapter.h"
#include "mm_event/mm_event.h"

void vh_customize_alloc_gfp(void *data, gfp_t *alloc_gfp, unsigned int order)
{
	if (*alloc_gfp & __GFP_MOVABLE)
		*alloc_gfp |= __GFP_CMA;
}

void xr_updata_page_mapcount(void *data, struct page *page, bool p1, bool p2, int *ptr, bool *p3)
{
	struct folio *folio = page_folio(page);

	if (folio_ref_count(folio) == 0)
		WARN_ON(1);

}

int xr_mem_adapter_init(void)
{
	int ret;

	ret = register_trace_android_vh_update_page_mapcount(xr_updata_page_mapcount, NULL);
	if (ret < 0)
		xrmem_err("xr_updata_page_mapcount register fail, ret %d.\n", ret);

	ret = register_trace_android_vh_customize_alloc_gfp(vh_customize_alloc_gfp, NULL);
	if (ret < 0)
		xrmem_err("vh_customize_alloc_gfp register fail, ret %d.\n", ret);

#ifdef CONFIG_XRING_KSHRINK_SLABD
	shrink_slabd_init();
#endif

#ifdef CONFIG_XRING_MM_ABORD
	abort_mm_opt_init();
#endif
#ifdef CONFIG_XRING_MM_EVENT
	mm_event_init();
#endif
	cma_track_init();
	pin_track_init();

	return ret;
}

void xr_mem_adapter_exit(void)
{
	pin_track_exit();
	cma_track_exit();
#ifdef CONFIG_XRING_MM_ABORD
	abort_mm_opt_exit();
#endif

#ifdef CONFIG_XRING_KSHRINK_SLABD
	shrink_slabd_exit();
#endif
#ifdef CONFIG_XRING_MM_EVENT
	mm_event_exit();
#endif
	unregister_trace_android_vh_customize_alloc_gfp(vh_customize_alloc_gfp, NULL);
	unregister_trace_android_vh_update_page_mapcount(xr_updata_page_mapcount, NULL);
}

MODULE_SOFTDEP("pre: miev");
module_init(xr_mem_adapter_init);
module_exit(xr_mem_adapter_exit);
