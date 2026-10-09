--  Ada95 bindings for the UIOP transport functions the four
--  examples/ada95 ORB examples call directly: For_Socket/Send_Message/
--  Call/Receive_Request/Receive_Reply (runtime/include/hace/uiop.h).
--  Mirrors bindings/rust/hace-corba/src/uiop.rs's scope exactly. See
--  docs/architecture/adr-0005-rust-ada-orb-bindings.md.

with Interfaces;
with Interfaces.C;
with System;
with Hace.Cdr;
with Hace.Giop;
with Hace.Hal;

package Hace.Uiop is

   Max_Datagram_Size : constant := 1472;

   type Status is
     (Ok,
      Err_Invalid_Argument,
      Err_Hal,
      Err_Too_Large,
      Err_Timeout);

   type Transport is limited private;

   --  Handle must stay open for as long as T is used -- T keeps its
   --  own copy of Handle's raw int value (see Hace.Hal.To_Raw), so
   --  Handle itself may go out of scope once this returns; only the
   --  underlying open socket must outlive T.
   procedure For_Socket (T : out Transport; Handle : Hace.Hal.Socket_Handle);

   --  Sends Message (built via Hace.Giop.Begin_Message + a per-type
   --  encode + Hace.Giop.Finish_Message) to Dest_Addr:Dest_Port. This
   --  is also the oneway primitive: a request encoded with
   --  Response_Expected = False, sent once via this procedure, with no
   --  call to Receive_Reply afterward, is a complete oneway call.
   procedure Send_Message
     (T          : Transport;
      Message    : in out Hace.Cdr.Buffer;
      Request_Id : Interfaces.Unsigned_32;
      Dest_Addr  : Hace.Hal.Ipv4_Address;
      Dest_Port  : Interfaces.Unsigned_16;
      Result     : out Status);

   --  The object key/operation/response_expected a dispatcher needs to
   --  route a received Request -- Request_Id alone isn't enough for
   --  that. Operation_Last is Operation's decoded length (Operation
   --  (Operation'First .. Operation_Last) is the actual name).
   type Request_Info is record
      Object_Key          : Hace.Cdr.Byte_Array (1 .. Hace.Giop.Max_Object_Key_Length);
      Object_Key_Length   : Natural;
      Operation           : String (1 .. Hace.Giop.Max_Operation_Name_Length);
      Operation_Last      : Natural;
      Response_Expected   : Boolean;
   end record;

   --  Waits up to Timeout_Ms for a complete Request (reassembling
   --  fragments transparently), for a server dispatching to a servant.
   procedure Receive_Request
     (T           : Transport;
      Timeout_Ms  : Interfaces.Unsigned_32;
      Body_Data   : out Hace.Cdr.Byte_Array;
      Body_Last   : out Integer;
      Request_Id  : out Interfaces.Unsigned_32;
      Src_Addr    : out Hace.Hal.Ipv4_Address;
      Src_Port    : out Interfaces.Unsigned_16;
      Info        : out Request_Info;
      Result      : out Status);

   --  Waits up to Timeout_Ms for a Reply matching Match_Request_Id --
   --  the Poller-style half of an asynchronous invocation (send via
   --  Send_Message, do other work, poll this on a short timeout until
   --  the reply lands).
   procedure Receive_Reply
     (T                : Transport;
      Match_Request_Id : Interfaces.Unsigned_32;
      Timeout_Ms       : Interfaces.Unsigned_32;
      Body_Data        : out Hace.Cdr.Byte_Array;
      Body_Last        : out Integer;
      Is_Exception     : out Boolean;
      Result           : out Status);

   --  The twoway client primitive: sends Request and waits
   --  Per_Attempt_Timeout_Ms for a matching Reply, resending up to
   --  Max_Attempts times before giving up with Err_Timeout.
   procedure Call
     (T                      : Transport;
      Request                : in out Hace.Cdr.Buffer;
      Request_Id             : Interfaces.Unsigned_32;
      Dest_Addr              : Hace.Hal.Ipv4_Address;
      Dest_Port              : Interfaces.Unsigned_16;
      Per_Attempt_Timeout_Ms : Interfaces.Unsigned_32;
      Max_Attempts           : Interfaces.Unsigned_32;
      Reply_Body             : out Hace.Cdr.Byte_Array;
      Reply_Body_Last        : out Integer;
      Is_Exception           : out Boolean;
      Result                 : out Status);

   --  Exposes T's opaque hace_uiop_transport_t storage address to
   --  Hace.Corba_Poa.Handle_One_Request, which passes it straight
   --  through to hace_corba_poa_handle_one_request. Not for
   --  application code -- mirrors Hace.Cdr.Raw_Address's rationale.
   function Raw_Address (T : Transport) return System.Address;

private

   use type Interfaces.C.int;

   --  hace_uiop_transport_t: two function pointers + a void* context --
   --  plain data, safe to mirror directly (same category as
   --  hace_corba_ior_t/hace_corba_system_exception_t), not opaque.
   type Raw_Transport is record
      Send_Fn : System.Address := System.Null_Address;
      Recv_Fn : System.Address := System.Null_Address;
      Context : System.Address := System.Null_Address;
   end record;
   pragma Convention (C, Raw_Transport);

   type Transport is limited record
      --  hace_uiop_transport_for_socket's returned Context pointer is
      --  set (by For_Socket) to point at THIS field, not at Handle
      --  itself: the production transport dereferences Context on
      --  every send/recv, so it must point at storage that outlives
      --  the call -- the Transport object itself, not a transient copy
      --  of Handle. Transport being limited guarantees it's always
      --  passed by reference (Ada RM 6.2), so 'Address inside this
      --  package's body always refers to the caller's real object.
      Handle_Copy : aliased Interfaces.C.int := -1;
      Raw         : Raw_Transport;
   end record;

end Hace.Uiop;
