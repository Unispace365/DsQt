# DsQt documentation guide

For a **new application**, start with [ProjectCloner and a published vcpkg package](../README.md#creating-a-new-project).
For an **existing application**, use [switching DsQt packages, versions, and Qt kits](articles/switching_dsqt_packages.md)
to adopt the registry or update its package selection. To add Waffles, follow the integration path in the
[Waffles implementation guide](waffles-implementation-guide.md#existing-dsqt-application-integrate-in-place).
ECPresenter and AssetViewer are reference implementations.

## Waffles

| Document | Use it for |
| --- | --- |
| [Overview](waffles-overview.md) | Terminology, implemented capabilities, and DsQt versus application responsibilities. |
| [Consolidated implementation guide](waffles-implementation-guide.md) | Build, schema preparation, Bridge wiring, templates, settings, startup, and validation. |
| [Backlog](waffles-backlog.md) | Completed work, open delivery tasks, ownership, and validation evidence. |
| [Source Tour](waffles-tour/index.html) | Detailed walkthroughs; open the HTML locally for its styling. The consolidated guide takes precedence where older recipes differ. |
| [Whiteboard reference](articles/whiteboard.md) | Drawing API, controls, history, export, and lifecycle. |

The Source Tour covers [stage](waffles-tour/stage.html), [viewers](waffles-tour/viewers.html),
[media](waffles-tour/media.html), [controls](waffles-tour/controls.html),
[content launcher](waffles-tour/content-launcher.html), [playlists](waffles-tour/playlists.html),
[glass and theming](waffles-tour/glass-and-theming.html), and
[demo recipes](waffles-tour/demo-and-recipes.html).

## Framework and deployment

| Document | Topic |
| --- | --- |
| [Best practices](articles/best_practices.md) | Framework conventions and application structure. |
| [DsQt vcpkg packages](vcpkg.md) | Recommended registry setup, local packages, caching, versioning, and publication. |
| [Switching packages, versions, and Qt kits](articles/switching_dsqt_packages.md) | Which CMake, preset, and manifest settings change, with working configuration examples. |
| [CMake user presets](articles/cmake_user_presets.md) | Machine-specific SDK paths, build configurations, and preset inheritance. |
| [Installed-library migration](articles/migration_subdirectory_to_library.md) | Legacy source-tree consumers moving to `find_package`; installed-library alternative and registry handoff. |
| [Settings](articles/settings_guide.md) | TOML loading, layering, overrides, and runtime access. |
| [Logging](articles/logging_guide.md) | Logging setup, locations, and diagnostics. |
| [Bridge usage](articles/bridge_usage.md) | Content synchronization and application access. |
| [Bridge database](articles/dsbridgedatabase_usage.md) | Database/query integration. |
| [Color system](articles/color_system.md) | Color configuration and conventions. |
| [SVG colors](articles/svg_color_reference.md) | SVG recoloring and asset references. |
| [Touch filter](articles/touch_filter.md) | Touch input filtering and configuration. |
| [Q-SYS](articles/qsys_usage.md) | Q-SYS control integration. |
| [Installer guide](articles/installer_guide.md) | Packaging and Windows deployment. |

[Generated Doxygen reference](https://unispace365.github.io/DsQt/) provides API
reference; its published revision may differ from your checkout. Source comments
and the implementation on your selected branch remain the authority.

## Related projects and design

- [DsQt](https://github.com/Unispace365/DsQt): reusable library and ProjectCloner.
- [ECPresenter](https://github.com/Unispace365/ECPresenter): Bridge-connected reference application.
- [Reusable Waffles schema](https://github.com/Unispace365/schema-waffles-dsqt): content definitions, contract, and adoption checks.
- [Figma design](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD) and [quick-menu UX](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=454-1302&m=dev).
