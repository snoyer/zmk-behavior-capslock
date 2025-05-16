if ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  # target_sources(app PRIVATE src/behaviors/behavior_capslock.c)
  target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_CAPSLOCK app PRIVATE src/behaviors/behavior_capslock.c)

  zephyr_include_directories(include)
endif()
