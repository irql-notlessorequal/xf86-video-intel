#include <stdbool.h>

#ifndef INTEL_DRIVER_H
#define INTEL_DRIVER_H

#if (defined(__cplusplus) && (__cplusplus >= 201703L)) || \
    (defined(__STDC_VERSION__) && (__STDC_VERSION__ > 201710L))
/* Standard C++17/C23 attribute */
#define FALLTHROUGH [[fallthrough]]
#elif HAS_CLANG_FALLTHROUGH
/* Clang++ specific */
#define FALLTHROUGH [[clang::fallthrough]]
#elif __has_attribute(fallthrough)
/* Non-standard but supported by at least gcc and clang */
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH do { } while(0)
#endif

struct xf86_platform_device;

#define INTEL_VERSION 4000
#define INTEL_NAME "intel"
#define INTEL_DRIVER_NAME "intel"

#define INTEL_VERSION_MAJOR PACKAGE_VERSION_MAJOR
#define INTEL_VERSION_MINOR PACKAGE_VERSION_MINOR
#define INTEL_VERSION_PATCH PACKAGE_VERSION_PATCHLEVEL

#define PCI_CHIP_I810		0x7121
#define PCI_CHIP_I810_DC100	0x7123
#define PCI_CHIP_I810_E		0x7125
#define PCI_CHIP_I815		0x1132

#define PCI_CHIP_I830_M		0x3577
#define PCI_CHIP_845_G		0x2562
#define PCI_CHIP_I854		0x358E
#define PCI_CHIP_I855_GM	0x3582
#define PCI_CHIP_I865_G		0x2572

#define PCI_CHIP_I915_G		0x2582
#define PCI_CHIP_I915_GM	0x2592
#define PCI_CHIP_E7221_G	0x258A
#define PCI_CHIP_I945_G		0x2772
#define PCI_CHIP_I945_GM        0x27A2
#define PCI_CHIP_I945_GME	0x27AE
#define PCI_CHIP_PINEVIEW_M	0xA011
#define PCI_CHIP_PINEVIEW_G	0xA001
#define PCI_CHIP_Q35_G		0x29B2
#define PCI_CHIP_G33_G		0x29C2
#define PCI_CHIP_Q33_G		0x29D2

#define PCI_CHIP_G35_G		0x2982
#define PCI_CHIP_I965_Q		0x2992
#define PCI_CHIP_I965_G		0x29A2
#define PCI_CHIP_I946_GZ	0x2972
#define PCI_CHIP_I965_GM        0x2A02
#define PCI_CHIP_I965_GME       0x2A12
#define PCI_CHIP_GM45_GM	0x2A42
#define PCI_CHIP_G45_E_G	0x2E02
#define PCI_CHIP_G45_G		0x2E22
#define PCI_CHIP_Q45_G		0x2E12
#define PCI_CHIP_G41_G		0x2E32
#define PCI_CHIP_B43_G		0x2E42
#define PCI_CHIP_B43_G1		0x2E92

#define PCI_CHIP_IRONLAKE_D_G		0x0042
#define PCI_CHIP_IRONLAKE_M_G		0x0046

#define PCI_CHIP_SANDYBRIDGE_GT1	0x0102
#define PCI_CHIP_SANDYBRIDGE_GT2	0x0112
#define PCI_CHIP_SANDYBRIDGE_GT2_PLUS	0x0122
#define PCI_CHIP_SANDYBRIDGE_M_GT1	0x0106
#define PCI_CHIP_SANDYBRIDGE_M_GT2	0x0116
#define PCI_CHIP_SANDYBRIDGE_M_GT2_PLUS	0x0126
#define PCI_CHIP_SANDYBRIDGE_S_GT	0x010A

#define PCI_CHIP_IVYBRIDGE_M_GT1	0x0156
#define PCI_CHIP_IVYBRIDGE_M_GT2	0x0166
#define PCI_CHIP_IVYBRIDGE_D_GT1	0x0152
#define PCI_CHIP_IVYBRIDGE_D_GT2	0x0162
#define PCI_CHIP_IVYBRIDGE_S_GT1	0x015a
#define PCI_CHIP_IVYBRIDGE_S_GT2	0x016a

#define PCI_CHIP_HASWELL_D_GT1		0x0402
#define PCI_CHIP_HASWELL_D_GT2		0x0412
#define PCI_CHIP_HASWELL_D_GT3		0x0422
#define PCI_CHIP_HASWELL_M_GT1		0x0406
#define PCI_CHIP_HASWELL_M_GT2		0x0416
#define PCI_CHIP_HASWELL_M_GT3		0x0426
#define PCI_CHIP_HASWELL_S_GT1		0x040A
#define PCI_CHIP_HASWELL_S_GT2		0x041A
#define PCI_CHIP_HASWELL_S_GT3		0x042A
#define PCI_CHIP_HASWELL_B_GT1		0x040B
#define PCI_CHIP_HASWELL_B_GT2		0x041B
#define PCI_CHIP_HASWELL_B_GT3		0x042B
#define PCI_CHIP_HASWELL_E_GT1		0x040E
#define PCI_CHIP_HASWELL_E_GT2		0x041E
#define PCI_CHIP_HASWELL_E_GT3		0x042E

#define PCI_CHIP_HASWELL_ULT_D_GT1	0x0A02
#define PCI_CHIP_HASWELL_ULT_D_GT2	0x0A12
#define PCI_CHIP_HASWELL_ULT_D_GT3	0x0A22
#define PCI_CHIP_HASWELL_ULT_M_GT1	0x0A06
#define PCI_CHIP_HASWELL_ULT_M_GT2	0x0A16
#define PCI_CHIP_HASWELL_ULT_M_GT3	0x0A26
#define PCI_CHIP_HASWELL_ULT_S_GT1	0x0A0A
#define PCI_CHIP_HASWELL_ULT_S_GT2	0x0A1A
#define PCI_CHIP_HASWELL_ULT_S_GT3	0x0A2A
#define PCI_CHIP_HASWELL_ULT_B_GT1	0x0A0B
#define PCI_CHIP_HASWELL_ULT_B_GT2	0x0A1B
#define PCI_CHIP_HASWELL_ULT_B_GT3	0x0A2B
#define PCI_CHIP_HASWELL_ULT_E_GT1	0x0A0E
#define PCI_CHIP_HASWELL_ULT_E_GT2	0x0A1E
#define PCI_CHIP_HASWELL_ULT_E_GT3	0x0A2E

#define PCI_CHIP_HASWELL_CRW_D_GT1	0x0D02
#define PCI_CHIP_HASWELL_CRW_D_GT2	0x0D12
#define PCI_CHIP_HASWELL_CRW_D_GT3	0x0D22
#define PCI_CHIP_HASWELL_CRW_M_GT1	0x0D06
#define PCI_CHIP_HASWELL_CRW_M_GT2	0x0D16
#define PCI_CHIP_HASWELL_CRW_M_GT3	0x0D26
#define PCI_CHIP_HASWELL_CRW_S_GT1	0x0D0A
#define PCI_CHIP_HASWELL_CRW_S_GT2	0x0D1A
#define PCI_CHIP_HASWELL_CRW_S_GT3	0x0D2A
#define PCI_CHIP_HASWELL_CRW_B_GT1	0x0D0B
#define PCI_CHIP_HASWELL_CRW_B_GT2	0x0D1B
#define PCI_CHIP_HASWELL_CRW_B_GT3	0x0D2B
#define PCI_CHIP_HASWELL_CRW_E_GT1	0x0D0E
#define PCI_CHIP_HASWELL_CRW_E_GT2	0x0D1E
#define PCI_CHIP_HASWELL_CRW_E_GT3	0x0D2E

struct intel_device_info
{
	/* 0-255 is more than enough. */
	uint8_t gen;

	/* These iGPUs should work fine with Y-tiling (Skylake and newer can even do scan-out) */
	bool prefer_y_tiling;

	/* Cherryview is an outlier that isn't supported by the modern Iris driver. */
	bool force_crocus_driver;

	/* Hardware is otherwise not supported. */
	bool force_ega;
};
struct intel_device;

int intel_entity_get_devid(int index);

int intel_open_device(int entity_num,
		      const struct pci_device *pci,
		      struct xf86_platform_device *dev);
void intel_close_device(int entity_num);
int __intel_peek_fd(ScrnInfoPtr scrn);
struct intel_device *intel_get_device(ScrnInfoPtr scrn, int *fd);
int intel_has_render_node(struct intel_device *dev);
const char *intel_get_master_name(struct intel_device *dev);
const char *intel_get_client_name(struct intel_device *dev);
int intel_get_client_fd(struct intel_device *dev);
int intel_get_device_id(struct intel_device *dev);
int intel_get_master(struct intel_device *dev);
int intel_put_master(struct intel_device *dev);
void intel_put_device(struct intel_device *dev);
char *intel_str_replace(char *orig, char *rep, char *with);
int intel_is_same_file(int fd1, int fd2);
int __get_render_node_count(void);
int __get_correct_render_node(struct intel_device *dev);
int __requires_drm_workaround(struct intel_device *dev);

void intel_detect_chipset(ScrnInfoPtr scrn, struct intel_device *dev);

#define IS_DEFAULT_ACCEL_METHOD(x) ({ \
	enum { NOACCEL, SNA, UXA, EGA } default_accel_method__ = DEFAULT_ACCEL_METHOD; \
	default_accel_method__ == x; \
})

#define hosted() (0)

#endif /* INTEL_DRIVER_H */
