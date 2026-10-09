--  Shared helpers for the examples/*/ada95 client/server pairs:
--  parsing an optional IOR string argument (falling back to a sensible
--  localhost default), and announcing a server's own IOR on startup.
--  Mirrors examples/common/c/example_common.h and
--  examples/common/rust/src/lib.rs. Not part of the runtime or its
--  DAL C boundary -- this is example/demo code, so unlike the
--  certified library it uses Ada.Text_IO freely.

with Interfaces;
with Hace.Cdr;
with Hace.Corba_Ior;
with Hace.Hal;

package Example_Common is

   --  Every example defaults to localhost; pass a real IOR string (as
   --  printed by a server) as argv(1) to run client/server on separate
   --  hosts instead.
   Loopback : constant Hace.Hal.Ipv4_Address := 16#7F00_0001#; -- 127.0.0.1

   function Default_Ior
     (Port : Interfaces.Unsigned_16; Object_Key : Hace.Cdr.Byte_Array)
      return Hace.Corba_Ior.Ior;

   procedure Announce_Server (Role : String; Ref : Hace.Corba_Ior.Ior);

   --  Opens a UDP socket on Port, builds the corresponding default
   --  IOR, and announces it. Success is False on failure (with a
   --  message already printed to standard error).
   procedure Server_Start
     (Role       : String;
      Port       : Interfaces.Unsigned_16;
      Object_Key : Hace.Cdr.Byte_Array;
      Socket     : out Hace.Hal.Socket_Handle;
      Ref        : out Hace.Corba_Ior.Ior;
      Success    : out Boolean);

   --  Parses Ada.Command_Line.Argument (1) as an IOR string (as
   --  printed by Announce_Server) if present, else falls back to the
   --  localhost default for Default_Port/Default_Object_Key.
   function Parse_Ior_Arg
     (Default_Port : Interfaces.Unsigned_16; Default_Object_Key : Hace.Cdr.Byte_Array)
      return Hace.Corba_Ior.Ior;

   --  Convenience: open a client-side UDP socket on an ephemeral port,
   --  printing a message and setting Success to False on failure.
   procedure Open_Client_Socket
     (Role : String; Socket : out Hace.Hal.Socket_Handle; Success : out Boolean);

end Example_Common;
