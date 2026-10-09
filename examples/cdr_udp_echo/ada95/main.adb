--  cdr_udp_echo (Ada95)
--
--  Encodes a small record with CDR, sends it over a raw UDP HAL socket to
--  itself, receives it back, and decodes it.
--
--  This is not an RPC example: hace-corba has no ORB/POA/GIOP layer yet
--  (see docs/roadmap.md), so there is no object invocation to
--  demonstrate. What's shown here -- CDR encode/decode and the HAL's UDP
--  sockets -- is what the Ada95 bindings actually cover today (see
--  bindings/ada95/hace_corba).

with Ada.Text_IO; use Ada.Text_IO;
with Ada.Streams; use Ada.Streams;
with Interfaces; use Interfaces;
with Interfaces.C;
with Hace.Cdr;
with Hace.Hal;

use type Hace.Cdr.Status;
use type Hace.Hal.Status;
use type Interfaces.C.double;

procedure Main is

   procedure Check (Ok : Boolean; What : String) is
   begin
      if not Ok then
         raise Program_Error with "FAILED: " & What;
      end if;
   end Check;

   Listen_Port : constant := 58301;
   Loopback    : constant Hace.Hal.Ipv4_Address := 16#7F00_0001#; -- 127.0.0.1

   --  The record being round-tripped, encoded field-by-field since the
   --  runtime only supports primitive CDR types so far (no generated
   --  struct marshaling until the idlc backends land -- see
   --  docs/roadmap.md, Phase 4).
   Reading_Id    : constant Unsigned_32 := 7;
   Reading_Value : constant Interfaces.C.double := 98.6;
   Reading_Valid : constant Boolean := True;

   Wire_Storage : Hace.Cdr.Byte_Array (0 .. 31) := (others => 0);
   Encode_Buf   : Hace.Cdr.Buffer;
   Cdr_Result   : Hace.Cdr.Status;
   Encoded_Len  : Natural;

   Send_Data : Stream_Element_Array (0 .. 31) := (others => 0);
   Socket     : Hace.Hal.Socket_Handle;
   Hal_Result : Hace.Hal.Status;

   Recv_Data : Stream_Element_Array (0 .. 31) := (others => 0);
   Recv_Last : Stream_Element_Offset;
   Src_Addr  : Hace.Hal.Ipv4_Address;
   Src_Port  : Unsigned_16;

   Decode_Storage : Hace.Cdr.Byte_Array (0 .. 31) := (others => 0);
   Decode_Buf     : Hace.Cdr.Buffer;

   Decoded_Id    : Unsigned_32;
   Decoded_Value : Interfaces.C.double;
   Decoded_Valid : Boolean;

begin
   --  Encode.
   Hace.Cdr.Init (Encode_Buf, Wire_Storage, Hace.Cdr.Big_Endian, Cdr_Result);
   Check (Cdr_Result = Hace.Cdr.Ok, "Cdr.Init (encode)");

   Hace.Cdr.Encode_U32 (Encode_Buf, Reading_Id, Cdr_Result);
   Check (Cdr_Result = Hace.Cdr.Ok, "Cdr.Encode_U32");

   Hace.Cdr.Encode_F64 (Encode_Buf, Reading_Value, Cdr_Result);
   Check (Cdr_Result = Hace.Cdr.Ok, "Cdr.Encode_F64");

   Hace.Cdr.Encode_Boolean (Encode_Buf, Reading_Valid, Cdr_Result);
   Check (Cdr_Result = Hace.Cdr.Ok, "Cdr.Encode_Boolean");

   Encoded_Len := Hace.Cdr.Position (Encode_Buf);
   Put_Line ("encoded" & Encoded_Len'Img & " bytes");

   for I in 0 .. Encoded_Len - 1 loop
      Send_Data (Stream_Element_Offset (I)) := Stream_Element (Wire_Storage (I));
   end loop;

   --  Send to self over loopback.
   Hace.Hal.Open (Socket, Listen_Port, Hal_Result);
   Check (Hal_Result = Hace.Hal.Ok, "Hal.Open");

   Hace.Hal.Send
     (Socket, Send_Data (0 .. Stream_Element_Offset (Encoded_Len) - 1), Loopback,
      Listen_Port, Hal_Result);
   Check (Hal_Result = Hace.Hal.Ok, "Hal.Send");

   --  Receive.
   Hace.Hal.Receive (Socket, Recv_Data, Recv_Last, Src_Addr, Src_Port, 1000, Hal_Result);
   Check (Hal_Result = Hace.Hal.Ok, "Hal.Receive");
   Put_Line ("received" & Stream_Element_Offset'Image (Recv_Last + 1) & " bytes");

   for I in 0 .. Recv_Last loop
      Decode_Storage (Natural (I)) := Hace.Cdr.Octet (Recv_Data (I));
   end loop;

   --  Decode.
   Hace.Cdr.Init (Decode_Buf, Decode_Storage, Hace.Cdr.Big_Endian, Cdr_Result);
   Check (Cdr_Result = Hace.Cdr.Ok, "Cdr.Init (decode)");

   Hace.Cdr.Decode_U32 (Decode_Buf, Decoded_Id, Cdr_Result);
   Check (Cdr_Result = Hace.Cdr.Ok, "Cdr.Decode_U32");

   Hace.Cdr.Decode_F64 (Decode_Buf, Decoded_Value, Cdr_Result);
   Check (Cdr_Result = Hace.Cdr.Ok, "Cdr.Decode_F64");

   Hace.Cdr.Decode_Boolean (Decode_Buf, Decoded_Valid, Cdr_Result);
   Check (Cdr_Result = Hace.Cdr.Ok, "Cdr.Decode_Boolean");

   Put_Line
     ("decoded: id=" & Decoded_Id'Img & " value=" & Decoded_Value'Img &
      " valid=" & Decoded_Valid'Img);

   Check (Decoded_Id = Reading_Id, "id round-trip");
   Check (abs (Decoded_Value - Reading_Value) < 0.0000001, "value round-trip");
   Check (Decoded_Valid = Reading_Valid, "valid round-trip");

   Put_Line ("round-trip OK");
end Main;
