#ifndef SNA_RENDER_H
#define SNA_RENDER_H

#include "compiler.h"

#include <picturestr.h>

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include "atomic.h"

#define GRADIENT_CACHE_SIZE 16

#define GXinvalid 0xff

struct sna;
struct sna_glyph;
struct sna_video;
struct sna_video_frame;
struct brw_compile;

struct sna_composite_rectangles {
	struct sna_coordinate {
		int16_t x, y;
	} src, mask, dst;
	int16_t width, height;
};

struct sna_composite_op
{
	fastcall void (*blt)(struct sna *sna, const struct sna_composite_op *op,
			     const struct sna_composite_rectangles *r);
	fastcall void (*box)(struct sna *sna,
			     const struct sna_composite_op *op,
			     const BoxRec *box);
	void (*boxes)(struct sna *sna, const struct sna_composite_op *op,
		      const BoxRec *box, int nbox);
	void (*thread_boxes)(struct sna *sna, const struct sna_composite_op *op,
			     const BoxRec *box, int nbox);
	void (*done)(struct sna *sna, const struct sna_composite_op *op);

	struct sna_damage **damage;

	uint32_t op;

	bool is_affine;
	bool has_component_alpha;
	bool need_magic_ca_pass;
	bool rb_reversed;

	struct
	{
		PixmapPtr pixmap; /* XXX */
		struct kgem_bo *bo;
		CARD32 format;
		int16_t x, y;
		uint16_t width, height;
	} dst;

	struct sna_composite_channel {
		struct kgem_bo *bo;
		PictTransform *transform;
		uint16_t width;
		uint16_t height;
		uint32_t pict_format;
		uint32_t card_format;
		uint32_t filter;
		uint32_t repeat;
		uint32_t is_affine : 1;
		uint32_t is_solid : 1;
		uint32_t is_linear : 1;
		uint32_t is_opaque : 1;
		uint32_t alpha_fixup : 1;
		uint32_t rb_reversed : 1;
		int16_t offset[2];
		float scale[2];

		pixman_transform_t embedded_transform;

		union {
			struct {
				float dx, dy, offset;
			} linear;
			struct {
				uint32_t pixel;
			} gen2;
			struct gen3_shader_channel {
				int type;
				uint32_t mode;
				uint32_t constants;
			} gen3;
		} u;
	} src, mask;

	int16_t floats_per_vertex;
	int16_t floats_per_rect;
	fastcall void (*prim_emit)(struct sna *sna,
				   const struct sna_composite_op *op,
				   const struct sna_composite_rectangles *r);
	fastcall void (*emit_boxes)(const struct sna_composite_op *op,
				    const BoxRec *box, int nbox,
				    float *v);

	struct sna_composite_redirect {
		struct kgem_bo *real_bo;
		struct sna_damage **real_damage, *damage;
		BoxRec box;
	} redirect;

	union {
		struct sna_blt_state
		{
			PixmapPtr src_pixmap;
			int16_t sx, sy;

			uint32_t cmd;
			uint32_t br13;
			uint32_t pixel;

			uint8_t bpp;
			uint8_t alu;

			bool inplace;
			bool overwrites;

			struct kgem_bo *bo[2];
			uint32_t pitch[2];
		} blt;

		struct {
			float constants[8];
			uint32_t num_constants;
		} gen3;

		struct {
			int wm_kernel;
			int ve_id;
		} gen4;

		struct {
			int16_t wm_kernel;
			int16_t ve_id;
		} gen5;

		struct {
			uint32_t flags;
		} gen6;

		struct {
			uint32_t flags;
		} gen7;

		struct {
			uint32_t flags;
		} gen8;

		struct {
			uint32_t flags;
			uint8_t wm_kernel;
		} gen9;
	} u;

	void *priv;
};

struct sna_opacity_box {
	BoxRec box;
	float alpha;
} tightly_packed;

struct sna_composite_spans_op {
	struct sna_composite_op base;

	fastcall void (*box)(struct sna *sna,
			     const struct sna_composite_spans_op *op,
			     const BoxRec *box,
			     float opacity);
	void (*boxes)(struct sna *sna,
		      const struct sna_composite_spans_op *op,
		      const BoxRec *box, int nbox,
		      float opacity);

	fastcall void (*thread_boxes)(struct sna *sna,
				      const struct sna_composite_spans_op *op,
				      const struct sna_opacity_box *box,
				      int nbox);

	fastcall void (*done)(struct sna *sna,
			      const struct sna_composite_spans_op *op);

	fastcall void (*prim_emit)(struct sna *sna,
				   const struct sna_composite_spans_op *op,
				   const BoxRec *box,
				   float opacity);
	fastcall void (*emit_boxes)(const struct sna_composite_spans_op *op,
				    const struct sna_opacity_box *box, int nbox,
				    float *v);
};

struct sna_fill_op {
	struct sna_composite_op base;

	void (*blt)(struct sna *sna, const struct sna_fill_op *op,
		    int16_t x, int16_t y, int16_t w, int16_t h);
	fastcall void (*box)(struct sna *sna,
			     const struct sna_fill_op *op,
			     const BoxRec *box);
	fastcall void (*boxes)(struct sna *sna,
			       const struct sna_fill_op *op,
			       const BoxRec *box,
			       int count);
	fastcall void (*points)(struct sna *sna,
			       const struct sna_fill_op *op,
			       int16_t dx, int16_t dy,
			       const DDXPointRec *points,
			       int count);
	void (*done)(struct sna *sna, const struct sna_fill_op *op);
};

struct sna_copy_op {
	struct sna_composite_op base;

	void (*blt)(struct sna *sna, const struct sna_copy_op *op,
		    int16_t sx, int16_t sy,
		    int16_t w, int16_t h,
		    int16_t dx, int16_t dy);
	void (*done)(struct sna *sna, const struct sna_copy_op *op);
};

struct sna_render {
	pthread_mutex_t lock;
	pthread_cond_t wait;
	int active;

	int max_3d_size;
	int max_3d_pitch;

	unsigned prefer_gpu;
#define PREFER_GPU_BLT 0x1
#define PREFER_GPU_RENDER 0x2
#define PREFER_GPU_SPANS 0x4

	bool (*composite)(struct sna *sna, uint8_t op,
			  PicturePtr dst, PicturePtr src, PicturePtr mask,
			  int16_t src_x, int16_t src_y,
			  int16_t msk_x, int16_t msk_y,
			  int16_t dst_x, int16_t dst_y,
			  int16_t w, int16_t h,
			  unsigned flags,
			  struct sna_composite_op *tmp);
#define COMPOSITE_PARTIAL	0x1
#define COMPOSITE_UPLOAD	0x40000000
#define COMPOSITE_FALLBACK	0x80000000

	bool (*check_composite_spans)(struct sna *sna, uint8_t op,
				      PicturePtr dst, PicturePtr src,
				      int16_t w, int16_t h, unsigned flags);
	bool (*composite_spans)(struct sna *sna, uint8_t op,
				PicturePtr dst, PicturePtr src,
				int16_t src_x, int16_t src_y,
				int16_t dst_x, int16_t dst_y,
				int16_t w, int16_t h,
				unsigned flags,
				struct sna_composite_spans_op *tmp);
#define COMPOSITE_SPANS_RECTILINEAR 0x1
#define COMPOSITE_SPANS_INPLACE_HINT 0x2

	bool (*video)(struct sna *sna,
		      struct sna_video *video,
		      struct sna_video_frame *frame,
		      RegionPtr dstRegion,
		      PixmapPtr pixmap);

	bool (*fill_boxes)(struct sna *sna,
			   CARD8 op,
			   PictFormat format,
			   const xRenderColor *color,
			   const DrawableRec *dst, struct kgem_bo *dst_bo,
			   const BoxRec *box, int n);
	bool (*fill)(struct sna *sna, uint8_t alu,
		     PixmapPtr dst, struct kgem_bo *dst_bo,
		     uint32_t color, unsigned flags,
		     struct sna_fill_op *tmp);
#define FILL_BOXES 0x1
#define FILL_POINTS 0x2
#define FILL_SPANS 0x4
	bool (*fill_one)(struct sna *sna, PixmapPtr dst, struct kgem_bo *dst_bo,
			 uint32_t color,
			 int16_t x1, int16_t y1, int16_t x2, int16_t y2,
			 uint8_t alu);
	bool (*clear)(struct sna *sna, PixmapPtr dst, struct kgem_bo *dst_bo);

	bool (*copy_boxes)(struct sna *sna, uint8_t alu,
			   const DrawableRec *src, struct kgem_bo *src_bo, int16_t src_dx, int16_t src_dy,
			   const DrawableRec *dst, struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
			   const BoxRec *box, int n, unsigned flags);
#define COPY_LAST 0x1
#define COPY_SYNC 0x2
#define COPY_NO_OVERLAP 0x4
#define COPY_SMALL 0x8
#define COPY_DRI 0x10
#define COPY_AVOID_BLT 0x20

	bool (*copy)(struct sna *sna, uint8_t alu,
		     PixmapPtr src, struct kgem_bo *src_bo,
		     PixmapPtr dst, struct kgem_bo *dst_bo,
		     struct sna_copy_op *op);

	void (*flush)(struct sna *sna);
	void (*reset)(struct sna *sna);
	void (*fini)(struct sna *sna);

	struct sna_alpha_cache {
		struct kgem_bo *cache_bo;
		struct kgem_bo *bo[256+7];
	} alpha_cache;

	struct sna_solid_cache
	{
		struct kgem_bo *cache_bo;
		struct kgem_bo *bo[1024];
		uint32_t color[1024];
		int last;
		int size;
		bool dirty;
	} solid_cache;

	struct {
		struct sna_gradient_cache
		{
			struct kgem_bo *bo;
			PictGradientStop *stops;
			int nstops;
		} cache[GRADIENT_CACHE_SIZE];
		int size;
	} gradient_cache;

	struct sna_glyph_cache{
		PicturePtr picture;
		struct sna_glyph **glyphs;
		uint16_t count;
		uint16_t evict;
	} glyph[2];
	pixman_image_t *white_image;
	PicturePtr white_picture;

	uint16_t vb_id;
	uint16_t vertex_offset;
	uint16_t vertex_start;
	uint16_t vertex_index;
	uint16_t vertex_used;
	uint16_t vertex_size;
	uint16_t vertex_reloc[16];
	int nvertex_reloc;

	struct kgem_bo *vbo;
	float *vertices;

	float vertex_data[1024];
};

struct gen2_render_state {
	uint32_t target;
	uint32_t logic_op_enabled;
	uint32_t ls1, ls2, vft;
	uint32_t diffuse;
	uint32_t specular;
	bool need_invariant;
};

struct gen3_render_state {
	uint32_t current_dst;
	uint32_t tex_count;
	uint32_t last_drawrect_limit;
	uint32_t last_target;
	uint32_t last_blend;
	uint32_t last_constants;
	uint32_t last_sampler;
	uint32_t last_shader;
	uint32_t last_diffuse;
	uint32_t last_specular;

	uint16_t last_vertex_offset;
	uint16_t floats_per_vertex;
	uint16_t last_floats_per_vertex;

	bool need_invariant;

	uint32_t tex_map[4];
	uint32_t tex_handle[2];
	uint32_t tex_delta[2];
};

struct gen4_render_state {
	struct kgem_bo *general_bo;

	uint32_t vs;
	uint32_t sf;
	uint32_t wm;
	uint32_t cc;

	int ve_id;
	uint32_t drawrect_offset;
	uint32_t drawrect_limit;
	uint32_t last_pipelined_pointers;
	uint16_t last_primitive;
	int16_t floats_per_vertex;
	uint16_t surface_table;

	bool needs_invariant;
	bool needs_urb;
};

struct gen5_render_state {
	struct kgem_bo *general_bo;

	uint32_t vs;
	uint32_t sf[2];
	uint32_t wm;
	uint32_t cc;

	int ve_id;
	uint32_t drawrect_offset;
	uint32_t drawrect_limit;
	uint32_t last_pipelined_pointers;
	uint16_t last_primitive;
	int16_t floats_per_vertex;
	uint16_t surface_table;

	bool needs_invariant;
};

enum {
	GEN6_WM_KERNEL_NOMASK = 0,
	GEN6_WM_KERNEL_NOMASK_P,

	GEN6_WM_KERNEL_MASK,
	GEN6_WM_KERNEL_MASK_P,

	GEN6_WM_KERNEL_MASKCA,
	GEN6_WM_KERNEL_MASKCA_P,

	GEN6_WM_KERNEL_MASKSA,
	GEN6_WM_KERNEL_MASKSA_P,

	GEN6_WM_KERNEL_OPACITY,
	GEN6_WM_KERNEL_OPACITY_P,

	GEN6_WM_KERNEL_VIDEO_PLANAR_BT601,
	GEN6_WM_KERNEL_VIDEO_NV12_BT601,
	GEN6_WM_KERNEL_VIDEO_PACKED_BT601,

	GEN6_WM_KERNEL_VIDEO_PLANAR_BT709,
	GEN6_WM_KERNEL_VIDEO_NV12_BT709,
	GEN6_WM_KERNEL_VIDEO_PACKED_BT709,

	GEN6_KERNEL_COUNT
};

struct gen6_render_state {
	const struct gt_info *info;
	struct kgem_bo *general_bo;

	unsigned gt;

	uint32_t vs_state;
	uint32_t sf_state;
	uint32_t sf_mask_state;
	uint32_t wm_state;
	uint32_t wm_kernel[GEN6_KERNEL_COUNT][3];

	uint32_t cc_blend;

	uint32_t drawrect_offset;
	uint32_t drawrect_limit;
	uint32_t blend;
	uint32_t samplers;
	uint32_t kernel;

	uint16_t num_sf_outputs;
	uint16_t ve_id;
	uint16_t last_primitive;
	int16_t floats_per_vertex;
	uint16_t surface_table;

	bool needs_invariant;
	bool first_state_packet;
};

enum {
	GEN7_WM_KERNEL_NOMASK = 0,
	GEN7_WM_KERNEL_NOMASK_P,

	GEN7_WM_KERNEL_MASK,
	GEN7_WM_KERNEL_MASK_P,

	GEN7_WM_KERNEL_MASKCA,
	GEN7_WM_KERNEL_MASKCA_P,

	GEN7_WM_KERNEL_MASKSA,
	GEN7_WM_KERNEL_MASKSA_P,

	GEN7_WM_KERNEL_OPACITY,
	GEN7_WM_KERNEL_OPACITY_P,

	GEN7_WM_KERNEL_VIDEO_PLANAR_BT601,
	GEN7_WM_KERNEL_VIDEO_NV12_BT601,
	GEN7_WM_KERNEL_VIDEO_PACKED_BT601,

	GEN7_WM_KERNEL_VIDEO_PLANAR_BT709,
	GEN7_WM_KERNEL_VIDEO_NV12_BT709,
	GEN7_WM_KERNEL_VIDEO_PACKED_BT709,

	GEN7_WM_KERNEL_VIDEO_RGB,
	GEN7_WM_KERNEL_COUNT
};

struct gen7_render_state {
	const struct gt_info *info;
	struct kgem_bo *general_bo;

	unsigned gt;

	uint32_t vs_state;
	uint32_t sf_state;
	uint32_t sf_mask_state;
	uint32_t wm_state;
	uint32_t wm_kernel[GEN7_WM_KERNEL_COUNT][3];

	uint32_t cc_blend;

	uint32_t drawrect_offset;
	uint32_t drawrect_limit;
	uint32_t blend;
	uint32_t samplers;
	uint32_t kernel;

	uint16_t num_sf_outputs;
	uint16_t ve_id;
	uint16_t last_primitive;
	int16_t floats_per_vertex;
	uint16_t surface_table;
	uint16_t pipe_controls_since_stall;

	bool needs_invariant;
	bool emit_flush;
};


enum {
	GEN8_WM_KERNEL_NOMASK = 0,
	GEN8_WM_KERNEL_NOMASK_P,

	GEN8_WM_KERNEL_MASK,
	GEN8_WM_KERNEL_MASK_P,

	GEN8_WM_KERNEL_MASKCA,
	GEN8_WM_KERNEL_MASKCA_P,

	GEN8_WM_KERNEL_MASKSA,
	GEN8_WM_KERNEL_MASKSA_P,

	GEN8_WM_KERNEL_OPACITY,
	GEN8_WM_KERNEL_OPACITY_P,

	GEN8_WM_KERNEL_VIDEO_PLANAR_BT601,
	GEN8_WM_KERNEL_VIDEO_NV12_BT601,
	GEN8_WM_KERNEL_VIDEO_PACKED_BT601,

	GEN8_WM_KERNEL_VIDEO_PLANAR_BT709,
	GEN8_WM_KERNEL_VIDEO_NV12_BT709,
	GEN8_WM_KERNEL_VIDEO_PACKED_BT709,

	GEN8_WM_KERNEL_VIDEO_RGB,
	GEN8_WM_KERNEL_COUNT
};

struct gen8_render_state {
	const struct gt_info *info;
	struct kgem_bo *general_bo;

	unsigned gt;

	uint32_t vs_state;
	uint32_t sf_state;
	uint32_t sf_mask_state;
	uint32_t wm_state;
	uint32_t wm_kernel[GEN8_WM_KERNEL_COUNT][3];

	uint32_t cc_blend;

	uint32_t drawrect_offset;
	uint32_t drawrect_limit;
	uint32_t blend;
	uint32_t samplers;
	uint32_t kernel;

	uint16_t num_sf_outputs;
	uint16_t ve_id;
	uint16_t last_primitive;
	int16_t floats_per_vertex;
	uint16_t surface_table;

	bool needs_invariant;
	bool emit_flush;
};

enum {
	GEN9_WM_KERNEL_NOMASK = 0,
	GEN9_WM_KERNEL_NOMASK_P,

	GEN9_WM_KERNEL_MASK,
	GEN9_WM_KERNEL_MASK_P,

	GEN9_WM_KERNEL_MASKCA,
	GEN9_WM_KERNEL_MASKCA_P,

	GEN9_WM_KERNEL_MASKSA,
	GEN9_WM_KERNEL_MASKSA_P,

	GEN9_WM_KERNEL_OPACITY,
	GEN9_WM_KERNEL_OPACITY_P,

	GEN9_WM_KERNEL_VIDEO_PLANAR_BT601,
	GEN9_WM_KERNEL_VIDEO_NV12_BT601,
	GEN9_WM_KERNEL_VIDEO_PACKED_BT601,

	GEN9_WM_KERNEL_VIDEO_PLANAR_BT709,
	GEN9_WM_KERNEL_VIDEO_NV12_BT709,
	GEN9_WM_KERNEL_VIDEO_PACKED_BT709,

	GEN9_WM_KERNEL_VIDEO_AYUV_BT601,
	GEN9_WM_KERNEL_VIDEO_AYUV_BT709,

	GEN9_WM_KERNEL_VIDEO_RGB,
	GEN9_WM_KERNEL_COUNT
};

struct gen9_render_state {
	const struct gt_info *info;
	struct kgem_bo *general_bo;

	unsigned gt;

	uint32_t vs_state;
	uint32_t sf_state;
	uint32_t sf_mask_state;
	uint32_t wm_state;
	uint32_t wm_kernel[GEN9_WM_KERNEL_COUNT][3];

	uint32_t cc_blend;

	uint32_t drawrect_offset;
	uint32_t drawrect_limit;
	uint32_t blend;
	uint32_t samplers;
	uint32_t kernel;

	uint16_t num_sf_outputs;
	uint16_t ve_id;
	uint16_t last_primitive;
	int16_t floats_per_vertex;
	uint16_t surface_table;

	bool needs_invariant;
	bool emit_flush;
	bool ve_dirty;
};

struct sna_static_stream {
	uint32_t size, used;
	uint8_t *data;
};

int sna_static_stream_init(struct sna_static_stream *stream);
uint32_t sna_static_stream_add(struct sna_static_stream *stream,
			       const void *data, uint32_t len, uint32_t align);
void *sna_static_stream_map(struct sna_static_stream *stream,
			    uint32_t len, uint32_t align);
uint32_t sna_static_stream_offsetof(struct sna_static_stream *stream,
				    void *ptr);
unsigned sna_static_stream_compile_sf(struct sna *sna,
				      struct sna_static_stream *stream,
				      bool (*compile)(struct brw_compile *));

unsigned sna_static_stream_compile_wm(struct sna *sna,
				      struct sna_static_stream *stream,
				      bool (*compile)(struct brw_compile *, int),
				      int width);
struct kgem_bo *sna_static_stream_fini(struct sna *sna,
				       struct sna_static_stream *stream);

struct kgem_bo *
sna_render_get_solid(struct sna *sna,
		     uint32_t color);

void
sna_render_flush_solid(struct sna *sna);

struct kgem_bo *
sna_render_get_gradient(struct sna *sna,
			PictGradient *pattern);

bool
sna_gradient_is_opaque(const PictGradient *gradient);

uint32_t sna_rgba_for_color(uint32_t color, int depth);
uint32_t sna_rgba_to_color(uint32_t rgba, uint32_t format);
bool sna_get_rgba_from_pixel(uint32_t pixel,
			     uint16_t *red,
			     uint16_t *green,
			     uint16_t *blue,
			     uint16_t *alpha,
			     uint32_t format);
bool sna_picture_is_solid(PicturePtr picture, uint32_t *color);

const char *no_render_init(struct sna *sna);
const char *gen2_render_init(struct sna *sna, const char *backend);
const char *gen3_render_init(struct sna *sna, const char *backend);
const char *gen4_render_init(struct sna *sna, const char *backend);
const char *gen5_render_init(struct sna *sna, const char *backend);
const char *gen6_render_init(struct sna *sna, const char *backend);
const char *gen7_render_init(struct sna *sna, const char *backend);
const char *gen8_render_init(struct sna *sna, const char *backend);
const char *gen9_render_init(struct sna *sna, const char *backend);

void sna_render_mark_wedged(struct sna *sna);

bool sna_tiling_composite(uint32_t op,
			  PicturePtr src,
			  PicturePtr mask,
			  PicturePtr dst,
			  int16_t src_x, int16_t src_y,
			  int16_t mask_x, int16_t mask_y,
			  int16_t dst_x, int16_t dst_y,
			  int16_t width, int16_t height,
			  struct sna_composite_op *tmp);
bool sna_tiling_composite_spans(uint32_t op,
				PicturePtr src,
				PicturePtr dst,
				int16_t src_x,  int16_t src_y,
				int16_t dst_x,  int16_t dst_y,
				int16_t width,  int16_t height,
				unsigned flags,
				struct sna_composite_spans_op *tmp);
bool sna_tiling_fill_boxes(struct sna *sna,
			   CARD8 op,
			   PictFormat format,
			   const xRenderColor *color,
			   const DrawableRec *dst, struct kgem_bo *dst_bo,
			   const BoxRec *box, int n);

bool sna_tiling_copy_boxes(struct sna *sna, uint8_t alu,
			   const DrawableRec *src, struct kgem_bo *src_bo, int16_t src_dx, int16_t src_dy,
			   const DrawableRec *dst, struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
			   const BoxRec *box, int n);

bool sna_tiling_blt_copy_boxes(struct sna *sna, uint8_t alu,
			       struct kgem_bo *src_bo, int16_t src_dx, int16_t src_dy,
			       struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
			       int bpp, const BoxRec *box, int nbox);

bool sna_tiling_blt_composite(struct sna *sna,
			      struct sna_composite_op *op,
			      struct kgem_bo *bo,
			      int bpp,
			      uint32_t alpha_fixup);

bool sna_blt_composite(struct sna *sna,
		       uint32_t op,
		       PicturePtr src,
		       PicturePtr dst,
		       int16_t src_x, int16_t src_y,
		       int16_t dst_x, int16_t dst_y,
		       int16_t width, int16_t height,
		       unsigned flags,
		       struct sna_composite_op *tmp);
bool sna_blt_composite__convert(struct sna *sna,
				int x, int y,
				int width, int height,
				struct sna_composite_op *tmp);

bool sna_blt_fill(struct sna *sna, uint8_t alu,
		  struct kgem_bo *bo,
		  int bpp,
		  uint32_t pixel,
		  struct sna_fill_op *fill);

bool sna_blt_copy(struct sna *sna, uint8_t alu,
		  struct kgem_bo *src,
		  struct kgem_bo *dst,
		  int bpp,
		  struct sna_copy_op *copy);

bool sna_blt_fill_boxes(struct sna *sna, uint8_t alu,
			struct kgem_bo *bo,
			int bpp,
			uint32_t pixel,
			const BoxRec *box, int n);

bool sna_blt_copy_boxes(struct sna *sna, uint8_t alu,
			struct kgem_bo *src_bo, int16_t src_dx, int16_t src_dy,
			struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
			int bpp,
			const BoxRec *box, int n);
bool sna_blt_copy_boxes__with_alpha(struct sna *sna, uint8_t alu,
				    struct kgem_bo *src_bo, int16_t src_dx, int16_t src_dy,
				    struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
				    int bpp, int alpha_fixup,
				    const BoxRec *box, int nbox);
bool sna_blt_copy_boxes_fallback(struct sna *sna, uint8_t alu,
				 const DrawableRec *src, struct kgem_bo *src_bo, int16_t src_dx, int16_t src_dy,
				 const DrawableRec *dst, struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
				 const BoxRec *box, int nbox);

bool memcpy_copy_boxes(struct sna *sna, uint8_t op,
		       const DrawableRec *src_draw, struct kgem_bo *src_bo, int16_t sx, int16_t sy,
		       const DrawableRec *dst_draw, struct kgem_bo *dst_bo, int16_t dx, int16_t dy,
		       const BoxRec *box, int n, unsigned flags);

bool _sna_get_pixel_from_rgba(uint32_t *pixel,
			     uint16_t red,
			     uint16_t green,
			     uint16_t blue,
			     uint16_t alpha,
			     uint32_t format);

static inline bool
sna_get_pixel_from_rgba(uint32_t * pixel,
			uint16_t red,
			uint16_t green,
			uint16_t blue,
			uint16_t alpha,
			uint32_t format)
{
	switch (format) {
	case PICT_x8r8g8b8:
		alpha = 0xffff;
		/* fall through */
	case PICT_a8r8g8b8:
		*pixel = ((alpha >> 8 << 24) |
			  (red >> 8 << 16) |
			  (green & 0xff00) |
			  (blue >> 8));
		return TRUE;
	case PICT_a8:
		*pixel = alpha >> 8;
		return TRUE;
	}

	return _sna_get_pixel_from_rgba(pixel, red, green, blue, alpha, format);
}

struct kgem_bo *
__sna_render_pixmap_bo(struct sna *sna,
		       PixmapPtr pixmap,
		       const BoxRec *box,
		       bool blt);

int
sna_render_pixmap_bo(struct sna *sna,
		     struct sna_composite_channel *channel,
		     PixmapPtr pixmap,
		     int16_t x, int16_t y,
		     int16_t w, int16_t h,
		     int16_t dst_x, int16_t dst_y);

bool
sna_render_pixmap_partial(struct sna *sna,
			  const DrawableRec *draw,
			  struct kgem_bo *bo,
			  struct sna_composite_channel *channel,
			  int16_t x, int16_t y,
			  int16_t w, int16_t h);

int
sna_render_picture_extract(struct sna *sna,
			   PicturePtr picture,
			   struct sna_composite_channel *channel,
			   int16_t x, int16_t y,
			   int16_t w, int16_t h,
			   int16_t dst_x, int16_t dst_y);

int
sna_render_picture_approximate_gradient(struct sna *sna,
					PicturePtr picture,
					struct sna_composite_channel *channel,
					int16_t x, int16_t y,
					int16_t w, int16_t h,
					int16_t dst_x, int16_t dst_y);

int
sna_render_picture_fixup(struct sna *sna,
			 PicturePtr picture,
			 struct sna_composite_channel *channel,
			 int16_t x, int16_t y,
			 int16_t w, int16_t h,
			 int16_t dst_x, int16_t dst_y);

int
sna_render_picture_convert(struct sna *sna,
			   PicturePtr picture,
			   struct sna_composite_channel *channel,
			   PixmapPtr pixmap,
			   int16_t x, int16_t y,
			   int16_t w, int16_t h,
			   int16_t dst_x, int16_t dst_y,
			   bool fixup_alpha);

inline static void sna_render_composite_redirect_init(struct sna_composite_op *op)
{
	struct sna_composite_redirect *t = &op->redirect;
	t->real_bo = NULL;
	t->damage = NULL;
}

bool
sna_render_composite_redirect(struct sna *sna,
			      struct sna_composite_op *op,
			      int x, int y, int width, int height,
			      bool partial);

void
sna_render_composite_redirect_done(struct sna *sna,
				   const struct sna_composite_op *op);

bool
sna_render_copy_boxes__overlap(struct sna *sna, uint8_t alu,
			       const DrawableRec *draw, struct kgem_bo *bo,
			       int16_t src_dx, int16_t src_dy,
			       int16_t dst_dx, int16_t dst_dy,
			       const BoxRec *box, int n, const BoxRec *extents);

bool
sna_composite_mask_is_opaque(PicturePtr mask);

void sna_vertex_init(struct sna *sna);

static inline void sna_vertex_lock(struct sna_render *r)
{
	pthread_mutex_lock(&r->lock);
}

static inline void sna_vertex_acquire__locked(struct sna_render *r)
{
	r->active++;
}

static inline void sna_vertex_unlock(struct sna_render *r)
{
	pthread_mutex_unlock(&r->lock);
}

static inline void sna_vertex_release__locked(struct sna_render *r)
{
	assert(r->active > 0);
	if (--r->active == 0)
		pthread_cond_signal(&r->wait);
}

static inline bool sna_vertex_wait__locked(struct sna_render *r)
{
	bool was_active = r->active;
	while (r->active)
		pthread_cond_wait(&r->wait, &r->lock);
	return was_active;
}

#define alphaless(format) PICT_FORMAT(PICT_FORMAT_BPP(format),		\
				      PICT_FORMAT_TYPE(format),		\
				      0,				\
				      PICT_FORMAT_R(format),		\
				      PICT_FORMAT_G(format),		\
				      PICT_FORMAT_B(format))

#endif /* SNA_RENDER_H */
