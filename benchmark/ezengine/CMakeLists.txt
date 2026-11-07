# QuestBench: the ezEngine implementation of the QuestMark benchmark (see NOTES.md, benchmark/SPEC.md).
# Built as an ezEngine "external project": tools/build-ezengine.sh passes this folder as EZ_EXTERNAL_PROJECT_1 to the engine's CMake run.
ez_cmake_init()

ez_requires_renderer()
ez_requires(EZ_3RDPARTY_JOLT_SUPPORT)

set(PROJECT_NAME QuestBench)

ez_create_target(APPLICATION ${PROJECT_NAME})

# Window app (no console): a console app would add a console window to the render loop. The log goes to the bench's own stdout file instead.
ez_make_windowapp(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
  PRIVATE
  GameEngine
  JoltPlugin
)

ez_add_renderers(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE QB_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Project")

add_dependencies(${PROJECT_NAME}
  JoltPlugin
)

# Engine commit recorded in env.txt (engine_tag): read at configure time from the checkout that is being built.
execute_process(COMMAND git -C "${EZ_ROOT}" rev-parse --short HEAD OUTPUT_VARIABLE QB_EZ_TAG OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT QB_EZ_TAG)
  set(QB_EZ_TAG "unknown")
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE QB_EZ_TAG="${QB_EZ_TAG}")

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
