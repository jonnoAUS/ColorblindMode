# Colorblind Mode

An accessibility mod that applies **color correction filters** to help players with color vision deficiencies distinguish between colors in Geometry Dash.

## How it works

The mod overlays a full-screen tint using multiplicative blending. Each filter type shifts the color balance by reducing specific channels, making problem color pairs (like red/green or blue/yellow) easier to tell apart. The effect is applied globally across all menus, gameplay, and the editor.

## Features

- **Quick-toggle button** on the main menu (top-right corner) for fast on/off
- **Adjustable strength**: dial in how strong the correction feels (0 - 100%)
- **Persists across screens**: the filter stays active through menus, gameplay, and the editor without dropping out on scene transitions
- **Gameplay indicator**: a small "CB" badge in the corner so you know the filter is on while playing
- **Instant settings**: changes to filter type or strength apply immediately, no restart needed

## Notes

This mod uses a channel-scaling approach rather than a full daltonization matrix. It won't perfectly simulate corrected vision, but it meaningfully shifts the color balance to improve contrast between problem color pairs. Adjust the strength slider to find what feels right for you.