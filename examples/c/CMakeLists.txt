# Real ORB-based example client/server pairs (Phase 5, see
# docs/roadmap.md). Each example is two executables; both link
# hace_corba_core and reach examples/c/common/example_common.h via a
# relative "../common/..." #include (no extra include dir needed).

add_executable(rpc_basic_server rpc_basic/server.c)
target_link_libraries(rpc_basic_server PRIVATE hace_corba_core)

add_executable(rpc_basic_client rpc_basic/client.c)
target_link_libraries(rpc_basic_client PRIVATE hace_corba_core)

add_executable(oneway_server oneway/server.c)
target_link_libraries(oneway_server PRIVATE hace_corba_core)

add_executable(oneway_client oneway/client.c)
target_link_libraries(oneway_client PRIVATE hace_corba_core)

add_executable(async_ami_server async_ami/server.c)
target_link_libraries(async_ami_server PRIVATE hace_corba_core)

add_executable(async_ami_client async_ami/client.c)
target_link_libraries(async_ami_client PRIVATE hace_corba_core)

add_executable(observer_attach_subject_server observer_attach/subject_server.c)
target_link_libraries(observer_attach_subject_server PRIVATE hace_corba_core)

add_executable(observer_attach_observer_client observer_attach/observer_client.c)
target_link_libraries(observer_attach_observer_client PRIVATE hace_corba_core)
