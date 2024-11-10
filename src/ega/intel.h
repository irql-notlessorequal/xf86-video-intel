#include "intel_bufmgr.h"
#include "i915_drm.h"

#define INTEL_INFO(intel) ((intel)->info)
#define IS_GENx(intel, X) (INTEL_INFO(intel)->gen >= 8*(X) && INTEL_INFO(intel)->gen < 8*((X)+1))
#define IS_GEN1(intel) IS_GENx(intel, 1)
#define IS_GEN2(intel) IS_GENx(intel, 2)
#define IS_GEN3(intel) IS_GENx(intel, 3)
#define IS_GEN4(intel) IS_GENx(intel, 4)
#define IS_GEN5(intel) IS_GENx(intel, 5)
#define IS_GEN6(intel) IS_GENx(intel, 6)
#define IS_GEN7(intel) IS_GENx(intel, 7)
#define IS_HSW(intel) (INTEL_INFO(intel)->gen == 075)

/* Some chips have specific errata (or limits) that we need to workaround. */
#define IS_I830(intel) (intel_get_device_id((intel)->dev) == PCI_CHIP_I830_M)
#define IS_845G(intel) (intel_get_device_id((intel)->dev) == PCI_CHIP_845_G)
#define IS_I865G(intel) (intel_get_device_id((intel)->dev) == PCI_CHIP_I865_G)

#define IS_I915G(pI810) (intel_get_device_id((intel)->dev) == PCI_CHIP_I915_G || intel_get_device_id((intel)->dev) == PCI_CHIP_E7221_G)
#define IS_I915GM(pI810) (intel_get_device_id((intel)->dev) == PCI_CHIP_I915_GM)

#define IS_965_Q(pI810) (intel_get_device_id((intel)->dev) == PCI_CHIP_I965_Q)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

#ifndef ALIGN
#define ALIGN(i,m)	(((i) + (m) - 1) & ~((m) - 1))
#endif

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif