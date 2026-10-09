--  Ada95 bindings for the three GIOP functions the four
--  examples/ada95 ORB examples call directly to build an outgoing
--  Request message (runtime/include/hace/giop.h): Begin_Message,
--  Encode_Request_Header, Finish_Message. hace_uiop_call/
--  hace_uiop_receive already decode headers internally and hand back
--  plain values, so this package has no need to bind hace_giop_decode_*
--  at all -- mirrors bindings/rust/hace-corba/src/giop.rs's scope
--  exactly. See docs/architecture/adr-0005-rust-ada-orb-bindings.md.

with Interfaces;
with Hace.Cdr;

package Hace.Giop is

   Max_Object_Key_Length     : constant := 32;
   Max_Operation_Name_Length : constant := 64;

   type Status is
     (Ok,
      Err_Buffer_Overflow,
      Err_Buffer_Underflow,
      Err_Invalid_Argument,
      Err_Bad_Magic,
      Err_Unsupported_Version,
      Err_Unsupported_Message_Type,
      Err_Unsupported_Addressing_Disposition,
      Err_Unsupported_Sync_Scope,
      Err_Nonempty_Service_Context);

   --  Sets Buf's byte order and reserves the GIOP header region. Call
   --  this first, before Encode_Request_Header and any argument bytes.
   procedure Begin_Message
     (Buf : in out Hace.Cdr.Buffer; Order : Hace.Cdr.Byte_Order; Result : out Status);

   --  Object_Key/Operation are copied into the fixed-capacity wire
   --  representation; Result is Err_Invalid_Argument if either doesn't
   --  fit Max_Object_Key_Length/Max_Operation_Name_Length.
   procedure Encode_Request_Header
     (Buf               : in out Hace.Cdr.Buffer;
      Request_Id        : Interfaces.Unsigned_32;
      Response_Expected : Boolean;
      Object_Key        : Hace.Cdr.Byte_Array;
      Operation         : String;
      Result            : out Status);

   --  Call once the request body is fully encoded: writes the real
   --  GIOP header now that the body length is known. Always stamps
   --  HACE_GIOP_MSG_REQUEST with no fragments -- the only message type
   --  any of the four examples build (a Reply is only ever built by
   --  corba_poa.c's own dispatcher, never by example code).
   procedure Finish_Message (Buf : in out Hace.Cdr.Buffer; Result : out Status);

end Hace.Giop;
