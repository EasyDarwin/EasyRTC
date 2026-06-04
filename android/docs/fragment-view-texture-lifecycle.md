# Fragment / View / SurfaceTexture Lifecycle Callback Order

## Normal Startup

```
onCreate
  → onCreateView
    → onViewCreated
      → onStart
        → onResume
          → [TextureView added to Window]
            → onSurfaceTextureAvailable
              → onSurfaceTextureUpdated (every frame)
```

## Go to Background (Home button)

```
onPause
  → onStop
```

## Return from Background

```
onStart
  → onResume
```

## Screen Off / On

```
[Screen off] onPause → onStop
[Screen on]  onStart → onResume
```

## Switch to Another Fragment (ViewPager / Navigation)

```
onPause
  → onStop
    → onDestroyView
      → onSurfaceTextureDestroyed    ← TextureView destroyed along with View
```

## Fragment Recreated (navigate back to page)

```
onCreateView
  → onViewCreated
    → onStart
      → onResume
        → onSurfaceTextureAvailable  ← New TextureView created
```

## Activity Destroyed (exit)

```
onPause
  → onStop
    → onDestroyView
      → onSurfaceTextureDestroyed
    → onDestroy
```

## Configuration Change (screen rotation)

```
onPause
  → onStop
    → onDestroyView
      → onSurfaceTextureDestroyed
    → onDestroy
      → onCreate
        → onCreateView
          → onViewCreated
            → onStart
              → onResume
                → onSurfaceTextureAvailable
```

## Key Points

- **`onSurfaceTextureAvailable`** always fires after `onResume` (View must complete layout/draw first)
- **`onSurfaceTextureDestroyed`** always fires during `onDestroyView` (when View is detached)
- **Screen off/on does NOT trigger View destruction** — no `onSurfaceTextureAvailable/Destroyed`, only `onPause → onStop → onStart → onResume`
- **Session/camera resources**: open in `onSurfaceTextureAvailable`, close in `onSurfaceTextureDestroyed`; `onResume` only handles error recovery
