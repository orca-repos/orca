add_library(shared_qtsingleapplication STATIC
  qtsingleapplication.cpp qtsingleapplication.h
  qtlocalpeer.cpp qtlocalpeer.h
)
target_link_libraries(shared_qtsingleapplication shared_qtlockedfile Qt6::Core Qt6::Network Qt6::Widgets)
target_include_directories(shared_qtsingleapplication PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if (WIN32)
  target_compile_definitions(shared_qtsingleapplication PRIVATE "QT_QTSINGLEAPPLICATION_EXPORT=__declspec(dllexport)")
endif()
