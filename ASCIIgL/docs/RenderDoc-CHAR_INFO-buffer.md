# CHAR_INFO buffer in RenderDoc

The **char_info** resource is the quantization pass render target. It stores one `uint2` per pixel:

- **.x** = glyph (Unicode code point, 16-bit)
- **.y** = attributes (16-bit, e.g. foreground/background)

Format: **DXGI_FORMAT_R16G16_UINT**.

## Why it looks solid yellow

RenderDoc displays 2D textures as RGB. For R16G16_UINT it maps:

- Channel 0 (glyph) → **Red**
- Channel 1 (attributes) → **Green**
- **Blue** = 0 (no third channel)

So you see `(R_norm, G_norm, 0)` — i.e. red + green with no blue. That appears **yellow** whenever both channels have similar non-zero values. If the scene has similar colors, the LUT gives similar (glyph, attr) for many pixels, so the whole buffer can look like a solid or banded yellow. **The contents are correct**; it’s just how the format is visualized.

## How to inspect the data

1. **Pixel inspection**  
   In the texture viewer, right‑click a pixel and use the value inspector. You should see the two 16‑bit values (glyph, attributes).

2. **Channel picker**  
   Use the texture viewer toolbar to show only the first or second channel (e.g. “R only” = glyph, “G only” = attributes) so you can see variation per channel.

3. **Custom visualization (optional)**  
   In RenderDoc you can use a custom texture visualization shader to decode R16G16_UINT (e.g. map glyph to hue and attr to brightness) if you want a more intuitive debug view.

No code change is required for correctness; the yellow appearance is expected for this format in the default RGB view.
