# Real ORB-based example client/server pairs (Phase 5/6, see
# docs/roadmap.md). Each example lives under examples/<name>/, grouped
# by example type first and language second (examples/<name>/c/,
# examples/<name>/rust/, examples/<name>/ada95/), all three sharing a
# single examples/<name>/<name>.idl reference spec. This file builds
# just the C side; each pair is two executables, both linking
# hace_corba_core and reaching examples/common/c/example_common.h via
# a relative "../../common/c/..." #include (no extra include dir
# needed).

add_executable(rpc_basic_server rpc_basic/c/server.c)
target_link_libraries(rpc_basic_server PRIVATE hace_corba_core)

add_executable(rpc_basic_client rpc_basic/c/client.c)
target_link_libraries(rpc_basic_client PRIVATE hace_corba_core)

add_executable(oneway_server oneway/c/server.c)
target_link_libraries(oneway_server PRIVATE hace_corba_core)

add_executable(oneway_client oneway/c/client.c)
target_link_libraries(oneway_client PRIVATE hace_corba_core)

add_executable(async_ami_server async_ami/c/server.c)
target_link_libraries(async_ami_server PRIVATE hace_corba_core)

add_executable(async_ami_client async_ami/c/client.c)
target_link_libraries(async_ami_client PRIVATE hace_corba_core)

add_executable(observer_attach_subject_server observer_attach/c/subject_server.c)
target_link_libraries(observer_attach_subject_server PRIVATE hace_corba_core)

add_executable(observer_attach_observer_client observer_attach/c/observer_client.c)
target_link_libraries(observer_attach_observer_client PRIVATE hace_corba_core)
