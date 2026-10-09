--  async_ami server implementation: activates a Worker object
--  (object key "work") whose compute(input) operation takes ~300ms
--  of simulated work (Delay, unlike C's hand-rolled busy-wait, since
--  Ada has a real delay statement) before replying with
--  input * input. The delay exists so client.adb's Poller-style loop
--  has something real to demonstrate. See ../../server.idl for the
--  interface this hand-implements.

package Worker_Impl is
   procedure Run;
end Worker_Impl;
