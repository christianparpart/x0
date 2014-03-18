include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)

if(ENABLE_PLUGIN_DIRECTOR)
  add_definitions(
    -pthread
    -std=c++0x
    -fvisibility=hidden
    -DPACKAGE_NAME="${PACKAGE_NAME}"
    -DPACKAGE_VERSION="${PACKAGE_VERSION}"
    -DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
    -DSYSCONFDIR="${SYSCONFDIR}"
    -DVERSION="${PACKAGE_VERSION}")

  if(ENABLE_DIRECTOR_CACHE)
    set(director_cache_SRC ObjectCache.cpp)
  endif(ENABLE_DIRECTOR_CACHE)

  set(director_SRC
    RequestNotes.cpp
    DirectorPlugin.cpp
    HealthMonitor.cpp
    ApiRequest.cpp
    BackendManager.cpp
    BackendCluster.cpp
    Director.cpp
    Backend.cpp
    Scheduler.cpp
    HttpBackend.cpp
    HttpHealthMonitor.cpp
    FastCgiBackend.cpp
    FastCgiHealthMonitor.cpp
    RoadWarrior.cpp
    HaproxyApi.cpp
    ${director_cache_SRC}
  )

  x0_plugin(director "${director_SRC}")
  target_link_libraries(director ${TBB_LIBRARIES})
endif()
