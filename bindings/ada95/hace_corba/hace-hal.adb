with Ada.Streams; use Ada.Streams;
with System;

package body Hace.Hal is

   use type Interfaces.C.int;

   --  ---- Raw C imports (runtime/src/hal/hal.h) ----

   type C_Time is record
      Seconds     : Interfaces.Unsigned_64;
      Nanoseconds : Interfaces.Unsigned_32;
   end record;
   pragma Convention (C, C_Time);

   function C_Time_Now (Time : out C_Time) return Interfaces.C.int;
   pragma Import (C, C_Time_Now, "hace_hal_time_now");

   function C_Socket_Open
     (Handle : out Interfaces.C.int; Local_Port : Interfaces.C.unsigned_short)
      return Interfaces.C.int;
   pragma Import (C, C_Socket_Open, "hace_hal_socket_open");

   function C_Socket_Close (Handle : Interfaces.C.int) return Interfaces.C.int;
   pragma Import (C, C_Socket_Close, "hace_hal_socket_close");

   function C_Socket_Send
     (Handle    : Interfaces.C.int;
      Data      : System.Address;
      Length    : Interfaces.C.size_t;
      Dest_Addr : Interfaces.Unsigned_32;
      Dest_Port : Interfaces.C.unsigned_short) return Interfaces.C.int;
   pragma Import (C, C_Socket_Send, "hace_hal_socket_send");

   function C_Socket_Recv
     (Handle     : Interfaces.C.int;
      Buffer     : System.Address;
      Capacity   : Interfaces.C.size_t;
      Out_Length : out Interfaces.C.size_t;
      Src_Addr   : out Interfaces.Unsigned_32;
      Src_Port   : out Interfaces.C.unsigned_short;
      Timeout_Ms : Interfaces.Unsigned_32) return Interfaces.C.int;
   pragma Import (C, C_Socket_Recv, "hace_hal_socket_recv");

   --  ---- helpers ----

   function To_Status (Raw : Interfaces.C.int) return Status is
   begin
      case Raw is
         when 0 => return Ok;
         when 1 => return Err;
         when 2 => return Err_Timeout;
         when 3 => return Err_Would_Block;
         when 4 => return Err_Invalid_Argument;
         when 5 => return Err_Not_Implemented;
         when 6 => return Err_Resource_Exhausted;
         when others =>
            raise Program_Error with
              "hace_hal_status_t returned an unrecognized code" &
              Interfaces.C.int'Image (Raw) &
              " -- the Ada binding is out of sync with runtime/src/hal/hal.h";
      end case;
   end To_Status;

   --  ---- public subprograms ----

   procedure Now (Time : out Monotonic_Time; Result : out Status) is
      Raw : C_Time := (Seconds => 0, Nanoseconds => 0);
   begin
      Result := To_Status (C_Time_Now (Raw));
      Time := (Seconds => Raw.Seconds, Nanoseconds => Raw.Nanoseconds);
   end Now;

   procedure Open
     (Handle     : out Socket_Handle;
      Local_Port : Interfaces.Unsigned_16;
      Result     : out Status)
   is
      Raw_Handle : Interfaces.C.int := -1;
   begin
      Result :=
        To_Status (C_Socket_Open (Raw_Handle, Interfaces.C.unsigned_short (Local_Port)));
      Handle := Socket_Handle (Raw_Handle);
   end Open;

   procedure Close (Handle : in out Socket_Handle; Result : out Status) is
   begin
      Result := To_Status (C_Socket_Close (Interfaces.C.int (Handle)));
      Handle := Invalid_Socket;
   end Close;

   function To_Raw (Handle : Socket_Handle) return Interfaces.C.int is
   begin
      return Interfaces.C.int (Handle);
   end To_Raw;

   procedure Send
     (Handle    : Socket_Handle;
      Data      : Ada.Streams.Stream_Element_Array;
      Dest_Addr : Ipv4_Address;
      Dest_Port : Interfaces.Unsigned_16;
      Result    : out Status)
   is
   begin
      Result :=
        To_Status
          (C_Socket_Send
             (Interfaces.C.int (Handle), Data (Data'First)'Address,
              Interfaces.C.size_t (Data'Length), Interfaces.Unsigned_32 (Dest_Addr),
              Interfaces.C.unsigned_short (Dest_Port)));
   end Send;

   procedure Receive
     (Handle     : Socket_Handle;
      Data       : out Ada.Streams.Stream_Element_Array;
      Last       : out Ada.Streams.Stream_Element_Offset;
      Src_Addr   : out Ipv4_Address;
      Src_Port   : out Interfaces.Unsigned_16;
      Timeout_Ms : Interfaces.Unsigned_32;
      Result     : out Status)
   is
      Out_Length   : Interfaces.C.size_t := 0;
      Raw_Src_Addr : Interfaces.Unsigned_32 := 0;
      Raw_Src_Port : Interfaces.C.unsigned_short := 0;
   begin
      Result :=
        To_Status
          (C_Socket_Recv
             (Interfaces.C.int (Handle), Data (Data'First)'Address,
              Interfaces.C.size_t (Data'Length), Out_Length, Raw_Src_Addr,
              Raw_Src_Port, Timeout_Ms));
      Last := Data'First + Stream_Element_Offset (Out_Length) - 1;
      Src_Addr := Ipv4_Address (Raw_Src_Addr);
      Src_Port := Interfaces.Unsigned_16 (Raw_Src_Port);
   end Receive;

end Hace.Hal;
