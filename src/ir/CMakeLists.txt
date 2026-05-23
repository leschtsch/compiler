FIND_PACKAGE(LLVM REQUIRED CONFIG)

llvm_map_components_to_libnames(llvm_libs core irreader support native)

ADD_LIBRARY(emit_ir STATIC emit_ir.cpp)
TARGET_COMPILE_DEFINITIONS(emit_ir PRIVATE ${LLVM_DEFINITIONS})
TARGET_INCLUDE_DIRECTORIES(emit_ir SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(emit_ir PRIVATE project_options ${llvm_libs})


