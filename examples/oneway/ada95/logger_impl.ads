--  oneway server implementation: activates a Logger object (object
--  key "log") whose log(message) operation is oneway -- it prints
--  the message and sends nothing back. See ../../server.idl for the
--  interface this hand-implements.

package Logger_Impl is
   procedure Run;
end Logger_Impl;
