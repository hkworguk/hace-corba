--  Unit tests for the hace-corba Ada95 bindings
--  (bindings/ada95/hace_corba), mirroring the coverage of
--  runtime/tests/test_cdr.c, runtime/tests/test_hal.c, and
--  bindings/rust/hace-corba's #[cfg(test)] suites: the same behavior
--  should hold no matter which binding calls into the C runtime.
--
--  Minimal, self-contained -- no AUnit dependency, matching the C
--  runtime's own dependency-free test_framework.h approach.

with Ada.Text_IO; use Ada.Text_IO;
with Ada.Streams; use Ada.Streams;
with Interfaces; use Interfaces;
with Interfaces.C;
with Hace.Cdr;
with Hace.Hal;

use type Hace.Cdr.Status;
use type Hace.Cdr.Octet;
use type Hace.Hal.Status;
use type Hace.Hal.Ipv4_Address;
use type Interfaces.C.double;

procedure Test_Hace_Corba is

   Failures : Natural := 0;
   Total    : Natural := 0;

   procedure Check (Ok : Boolean; Name : String) is
   begin
      Total := Total + 1;
      if Ok then
         Put_Line ("  " & Name & " ... PASS");
      else
         Put_Line ("  " & Name & " ... FAIL");
         Failures := Failures + 1;
      end if;
   end Check;

   procedure Test_Octet_Roundtrip is
      Storage : Hace.Cdr.Byte_Array (0 .. 7) := (others => 0);
      Buf     : Hace.Cdr.Buffer;
      Result  : Hace.Cdr.Status;
      Value   : Hace.Cdr.Octet;
   begin
      Hace.Cdr.Init (Buf, Storage, Hace.Cdr.Big_Endian, Result);
      Hace.Cdr.Encode_Octet (Buf, 16#42#, Result);
      Check (Result = Hace.Cdr.Ok, "octet_roundtrip: encode");
      Check (Hace.Cdr.Position (Buf) = 1, "octet_roundtrip: position after encode");

      Hace.Cdr.Set_Position (Buf, 0, Result);
      Hace.Cdr.Decode_Octet (Buf, Value, Result);
      Check (Result = Hace.Cdr.Ok, "octet_roundtrip: decode");
      Check (Value = 16#42#, "octet_roundtrip: value");
   end Test_Octet_Roundtrip;

   procedure Test_Boolean_Roundtrip is
      Storage : Hace.Cdr.Byte_Array (0 .. 7) := (others => 0);
      Buf     : Hace.Cdr.Buffer;
      Result  : Hace.Cdr.Status;
      Value   : Boolean;
   begin
      Hace.Cdr.Init (Buf, Storage, Hace.Cdr.Little_Endian, Result);
      Hace.Cdr.Encode_Boolean (Buf, True, Result);
      Hace.Cdr.Encode_Boolean (Buf, False, Result);
      Check (Result = Hace.Cdr.Ok, "boolean_roundtrip: encode");

      Hace.Cdr.Set_Position (Buf, 0, Result);
      Hace.Cdr.Decode_Boolean (Buf, Value, Result);
      Check (Result = Hace.Cdr.Ok and Value, "boolean_roundtrip: first value true");
      Hace.Cdr.Decode_Boolean (Buf, Value, Result);
      Check (Result = Hace.Cdr.Ok and not Value, "boolean_roundtrip: second value false");
   end Test_Boolean_Roundtrip;

   procedure Test_U16_Alignment is
      Storage : Hace.Cdr.Byte_Array (0 .. 7) := (others => 0);
      Buf     : Hace.Cdr.Buffer;
      Result  : Hace.Cdr.Status;
      Octet_Value : Hace.Cdr.Octet;
      U16_Value   : Unsigned_16;
   begin
      Hace.Cdr.Init (Buf, Storage, Hace.Cdr.Big_Endian, Result);
      Hace.Cdr.Encode_Octet (Buf, 16#AA#, Result);
      Hace.Cdr.Encode_U16 (Buf, 16#1234#, Result);
      Check (Result = Hace.Cdr.Ok, "u16_alignment: encode");
      --  one octet + one pad byte + a 2-byte-aligned u16 == position 4
      Check (Hace.Cdr.Position (Buf) = 4, "u16_alignment: position");

      Hace.Cdr.Set_Position (Buf, 0, Result);
      Hace.Cdr.Decode_Octet (Buf, Octet_Value, Result);
      Hace.Cdr.Decode_U16 (Buf, U16_Value, Result);
      Check (Result = Hace.Cdr.Ok and Octet_Value = 16#AA# and U16_Value = 16#1234#,
             "u16_alignment: roundtrip");
   end Test_U16_Alignment;

   procedure Test_U32_Byte_Order is
      Storage_Be : Hace.Cdr.Byte_Array (0 .. 7) := (others => 0);
      Storage_Le : Hace.Cdr.Byte_Array (0 .. 7) := (others => 0);
      Buf_Be, Buf_Le : Hace.Cdr.Buffer;
      Result : Hace.Cdr.Status;
      Out_Be, Out_Le : Unsigned_32;
   begin
      Hace.Cdr.Init (Buf_Be, Storage_Be, Hace.Cdr.Big_Endian, Result);
      Hace.Cdr.Init (Buf_Le, Storage_Le, Hace.Cdr.Little_Endian, Result);

      Hace.Cdr.Encode_U32 (Buf_Be, 16#01020304#, Result);
      Hace.Cdr.Encode_U32 (Buf_Le, 16#01020304#, Result);

      Check (Storage_Be (0) = 16#01# and Storage_Be (3) = 16#04#,
             "u32_byte_order: big-endian bytes");
      Check (Storage_Le (0) = 16#04# and Storage_Le (3) = 16#01#,
             "u32_byte_order: little-endian bytes");

      Hace.Cdr.Set_Position (Buf_Be, 0, Result);
      Hace.Cdr.Set_Position (Buf_Le, 0, Result);
      Hace.Cdr.Decode_U32 (Buf_Be, Out_Be, Result);
      Hace.Cdr.Decode_U32 (Buf_Le, Out_Le, Result);
      Check (Out_Be = 16#01020304# and Out_Le = 16#01020304#,
             "u32_byte_order: roundtrip both orders");
   end Test_U32_Byte_Order;

   procedure Test_I64_Roundtrip is
      Storage : Hace.Cdr.Byte_Array (0 .. 15) := (others => 0);
      Buf     : Hace.Cdr.Buffer;
      Result  : Hace.Cdr.Status;
      Value   : Interfaces.Integer_64;
   begin
      Hace.Cdr.Init (Buf, Storage, Hace.Cdr.Big_Endian, Result);
      Hace.Cdr.Encode_I64 (Buf, -123_456_789_012_345, Result);
      Check (Result = Hace.Cdr.Ok and Hace.Cdr.Position (Buf) = 8,
             "i64_roundtrip: encode + position");

      Hace.Cdr.Set_Position (Buf, 0, Result);
      Hace.Cdr.Decode_I64 (Buf, Value, Result);
      Check (Result = Hace.Cdr.Ok and Value = -123_456_789_012_345,
             "i64_roundtrip: decode");
   end Test_I64_Roundtrip;

   procedure Test_F64_Roundtrip is
      Storage : Hace.Cdr.Byte_Array (0 .. 15) := (others => 0);
      Buf     : Hace.Cdr.Buffer;
      Result  : Hace.Cdr.Status;
      Value   : Interfaces.C.double;
      Pi      : constant Interfaces.C.double := 3.1415926535;
   begin
      Hace.Cdr.Init (Buf, Storage, Hace.Cdr.Little_Endian, Result);
      Hace.Cdr.Encode_F64 (Buf, Pi, Result);

      Hace.Cdr.Set_Position (Buf, 0, Result);
      Hace.Cdr.Decode_F64 (Buf, Value, Result);
      Check (Result = Hace.Cdr.Ok and abs (Value - Pi) < 1.0E-10,
             "f64_roundtrip: decode");
   end Test_F64_Roundtrip;

   procedure Test_Encode_Overflow_Is_Rejected is
      Storage : Hace.Cdr.Byte_Array (0 .. 1) := (others => 0);
      Buf     : Hace.Cdr.Buffer;
      Result  : Hace.Cdr.Status;
   begin
      Hace.Cdr.Init (Buf, Storage, Hace.Cdr.Big_Endian, Result);
      Hace.Cdr.Encode_U32 (Buf, 1, Result);
      Check (Result = Hace.Cdr.Err_Buffer_Overflow, "encode_overflow: rejected");
      Check (Hace.Cdr.Position (Buf) = 0, "encode_overflow: position unchanged");
   end Test_Encode_Overflow_Is_Rejected;

   procedure Test_Decode_Underflow_Is_Rejected is
      Storage : Hace.Cdr.Byte_Array (0 .. 1) := (others => 0);
      Buf     : Hace.Cdr.Buffer;
      Result  : Hace.Cdr.Status;
      Value   : Unsigned_32;
   begin
      Hace.Cdr.Init (Buf, Storage, Hace.Cdr.Big_Endian, Result);
      Hace.Cdr.Decode_U32 (Buf, Value, Result);
      Check (Result = Hace.Cdr.Err_Buffer_Underflow, "decode_underflow: rejected");
   end Test_Decode_Underflow_Is_Rejected;

   procedure Test_Set_Position_Past_Capacity_Is_Rejected is
      Storage : Hace.Cdr.Byte_Array (0 .. 3) := (others => 0);
      Buf     : Hace.Cdr.Buffer;
      Result  : Hace.Cdr.Status;
   begin
      Hace.Cdr.Init (Buf, Storage, Hace.Cdr.Big_Endian, Result);
      Hace.Cdr.Set_Position (Buf, 5, Result);
      Check (Result = Hace.Cdr.Err_Invalid_Argument, "set_position_past_capacity: rejected");
   end Test_Set_Position_Past_Capacity_Is_Rejected;

   procedure Test_Monotonic_Time_Moves_Forward is
      T1, T2 : Hace.Hal.Monotonic_Time;
      R1, R2 : Hace.Hal.Status;
   begin
      Hace.Hal.Now (T1, R1);
      Hace.Hal.Now (T2, R2);
      Check (R1 = Hace.Hal.Ok and R2 = Hace.Hal.Ok, "monotonic_time: calls succeed");
      Check
        (T2.Seconds > T1.Seconds or
         (T2.Seconds = T1.Seconds and T2.Nanoseconds >= T1.Nanoseconds),
         "monotonic_time: moves forward");
   end Test_Monotonic_Time_Moves_Forward;

   procedure Test_Udp_Loopback_Roundtrip is
      Receiver, Sender : Hace.Hal.Socket_Handle;
      R        : Hace.Hal.Status;
      Message  : constant Stream_Element_Array := (1, 2, 3, 4, 5);
      Recv_Buf : Stream_Element_Array (0 .. 15) := (others => 0);
      Last     : Stream_Element_Offset;
      Src_Addr : Hace.Hal.Ipv4_Address;
      Src_Port : Unsigned_16;
      Loopback : constant Hace.Hal.Ipv4_Address := 16#7F00_0001#;
   begin
      Hace.Hal.Open (Receiver, 58311, R);
      Check (R = Hace.Hal.Ok, "udp_loopback: open receiver");
      Hace.Hal.Open (Sender, 58312, R);
      Check (R = Hace.Hal.Ok, "udp_loopback: open sender");

      Hace.Hal.Send (Sender, Message, Loopback, 58311, R);
      Check (R = Hace.Hal.Ok, "udp_loopback: send");

      Hace.Hal.Receive (Receiver, Recv_Buf, Last, Src_Addr, Src_Port, 1000, R);
      Check (R = Hace.Hal.Ok, "udp_loopback: receive");
      Check (Last = Message'Length - 1, "udp_loopback: length matches");
      Check (Recv_Buf (0 .. Last) = Message, "udp_loopback: content matches");
      Check (Src_Addr = Loopback, "udp_loopback: sender address matches");

      Hace.Hal.Close (Receiver, R);
      Hace.Hal.Close (Sender, R);
   end Test_Udp_Loopback_Roundtrip;

   procedure Test_Udp_Recv_Would_Block_When_Idle is
      Sock     : Hace.Hal.Socket_Handle;
      R        : Hace.Hal.Status;
      Buf      : Stream_Element_Array (0 .. 15);
      Last     : Stream_Element_Offset;
      Src_Addr : Hace.Hal.Ipv4_Address;
      Src_Port : Unsigned_16;
   begin
      Hace.Hal.Open (Sock, 58313, R);
      Check (R = Hace.Hal.Ok, "udp_would_block: open");

      Hace.Hal.Receive (Sock, Buf, Last, Src_Addr, Src_Port, 0, R);
      Check (R = Hace.Hal.Err_Would_Block, "udp_would_block: result");

      Hace.Hal.Close (Sock, R);
   end Test_Udp_Recv_Would_Block_When_Idle;

begin
   Put_Line ("hace-corba Ada95 bindings unit tests");
   Test_Octet_Roundtrip;
   Test_Boolean_Roundtrip;
   Test_U16_Alignment;
   Test_U32_Byte_Order;
   Test_I64_Roundtrip;
   Test_F64_Roundtrip;
   Test_Encode_Overflow_Is_Rejected;
   Test_Decode_Underflow_Is_Rejected;
   Test_Set_Position_Past_Capacity_Is_Rejected;
   Test_Monotonic_Time_Moves_Forward;
   Test_Udp_Loopback_Roundtrip;
   Test_Udp_Recv_Would_Block_When_Idle;

   Put_Line (Total'Img & " total," & Failures'Img & " failed");
   if Failures > 0 then
      raise Program_Error with "one or more hace-corba Ada95 binding tests failed";
   end if;
end Test_Hace_Corba;
