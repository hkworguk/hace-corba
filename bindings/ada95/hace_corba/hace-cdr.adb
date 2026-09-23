with System;

package body Hace.Cdr is

   use type Interfaces.C.int;
   use type Interfaces.C.size_t;

   --  ---- Raw C imports (runtime/include/hace/cdr.h) ----
   --
   --  Buf is always passed as the address of a Buffer's opaque Raw
   --  storage. Per Ada RM Annex B.3, scalar `out` parameters of a
   --  subprogram imported with C convention are passed by reference
   --  (as a pointer), matching hace_cdr.h's `T *out` decode signatures
   --  -- no manual pointer plumbing is needed for those.

   function C_Buffer_Sizeof return Interfaces.C.size_t;
   pragma Import (C, C_Buffer_Sizeof, "hace_cdr_buffer_sizeof");

   function C_Buffer_Init
     (Buf : System.Address; Storage : System.Address;
      Capacity : Interfaces.C.size_t; Order : Interfaces.C.int)
      return Interfaces.C.int;
   pragma Import (C, C_Buffer_Init, "hace_cdr_buffer_init");

   function C_Buffer_Position (Buf : System.Address) return Interfaces.C.size_t;
   pragma Import (C, C_Buffer_Position, "hace_cdr_buffer_position");

   function C_Buffer_Capacity (Buf : System.Address) return Interfaces.C.size_t;
   pragma Import (C, C_Buffer_Capacity, "hace_cdr_buffer_capacity");

   function C_Buffer_Set_Position
     (Buf : System.Address; Position : Interfaces.C.size_t) return Interfaces.C.int;
   pragma Import (C, C_Buffer_Set_Position, "hace_cdr_buffer_set_position");

   function C_Encode_Octet
     (Buf : System.Address; Value : Interfaces.C.unsigned_char) return Interfaces.C.int;
   pragma Import (C, C_Encode_Octet, "hace_cdr_encode_octet");

   function C_Decode_Octet
     (Buf : System.Address; Value : out Interfaces.C.unsigned_char) return Interfaces.C.int;
   pragma Import (C, C_Decode_Octet, "hace_cdr_decode_octet");

   function C_Encode_Bool
     (Buf : System.Address; Value : Interfaces.C.int) return Interfaces.C.int;
   pragma Import (C, C_Encode_Bool, "hace_cdr_encode_bool");

   function C_Decode_Bool
     (Buf : System.Address; Value : out Interfaces.C.int) return Interfaces.C.int;
   pragma Import (C, C_Decode_Bool, "hace_cdr_decode_bool");

   function C_Encode_U16
     (Buf : System.Address; Value : Interfaces.Unsigned_16) return Interfaces.C.int;
   pragma Import (C, C_Encode_U16, "hace_cdr_encode_u16");

   function C_Decode_U16
     (Buf : System.Address; Value : out Interfaces.Unsigned_16) return Interfaces.C.int;
   pragma Import (C, C_Decode_U16, "hace_cdr_decode_u16");

   function C_Encode_I16
     (Buf : System.Address; Value : Interfaces.Integer_16) return Interfaces.C.int;
   pragma Import (C, C_Encode_I16, "hace_cdr_encode_i16");

   function C_Decode_I16
     (Buf : System.Address; Value : out Interfaces.Integer_16) return Interfaces.C.int;
   pragma Import (C, C_Decode_I16, "hace_cdr_decode_i16");

   function C_Encode_U32
     (Buf : System.Address; Value : Interfaces.Unsigned_32) return Interfaces.C.int;
   pragma Import (C, C_Encode_U32, "hace_cdr_encode_u32");

   function C_Decode_U32
     (Buf : System.Address; Value : out Interfaces.Unsigned_32) return Interfaces.C.int;
   pragma Import (C, C_Decode_U32, "hace_cdr_decode_u32");

   function C_Encode_I32
     (Buf : System.Address; Value : Interfaces.Integer_32) return Interfaces.C.int;
   pragma Import (C, C_Encode_I32, "hace_cdr_encode_i32");

   function C_Decode_I32
     (Buf : System.Address; Value : out Interfaces.Integer_32) return Interfaces.C.int;
   pragma Import (C, C_Decode_I32, "hace_cdr_decode_i32");

   function C_Encode_U64
     (Buf : System.Address; Value : Interfaces.Unsigned_64) return Interfaces.C.int;
   pragma Import (C, C_Encode_U64, "hace_cdr_encode_u64");

   function C_Decode_U64
     (Buf : System.Address; Value : out Interfaces.Unsigned_64) return Interfaces.C.int;
   pragma Import (C, C_Decode_U64, "hace_cdr_decode_u64");

   function C_Encode_I64
     (Buf : System.Address; Value : Interfaces.Integer_64) return Interfaces.C.int;
   pragma Import (C, C_Encode_I64, "hace_cdr_encode_i64");

   function C_Decode_I64
     (Buf : System.Address; Value : out Interfaces.Integer_64) return Interfaces.C.int;
   pragma Import (C, C_Decode_I64, "hace_cdr_decode_i64");

   function C_Encode_F32
     (Buf : System.Address; Value : Interfaces.C.C_float) return Interfaces.C.int;
   pragma Import (C, C_Encode_F32, "hace_cdr_encode_f32");

   function C_Decode_F32
     (Buf : System.Address; Value : out Interfaces.C.C_float) return Interfaces.C.int;
   pragma Import (C, C_Decode_F32, "hace_cdr_decode_f32");

   function C_Encode_F64
     (Buf : System.Address; Value : Interfaces.C.double) return Interfaces.C.int;
   pragma Import (C, C_Encode_F64, "hace_cdr_encode_f64");

   function C_Decode_F64
     (Buf : System.Address; Value : out Interfaces.C.double) return Interfaces.C.int;
   pragma Import (C, C_Decode_F64, "hace_cdr_decode_f64");

   --  ---- helpers ----

   function To_Status (Raw : Interfaces.C.int) return Status is
   begin
      case Raw is
         when 0 => return Ok;
         when 1 => return Err_Buffer_Overflow;
         when 2 => return Err_Buffer_Underflow;
         when 3 => return Err_Invalid_Argument;
         when others =>
            raise Program_Error with
              "hace_cdr_status_t returned an unrecognized code" &
              Interfaces.C.int'Image (Raw) &
              " -- the Ada binding is out of sync with runtime/include/hace/cdr.h";
      end case;
   end To_Status;

   function To_C (Order : Byte_Order) return Interfaces.C.int is
   begin
      case Order is
         when Big_Endian    => return 0;
         when Little_Endian => return 1;
      end case;
   end To_C;

   --  ---- public subprograms ----

   procedure Init
     (Buf     : out Buffer;
      Storage : in out Byte_Array;
      Order   : Byte_Order;
      Result  : out Status)
   is
      Native_Size : constant Interfaces.C.size_t := C_Buffer_Sizeof;
   begin
      if Native_Size > Interfaces.C.size_t (Opaque_Size) then
         raise Buffer_Sizeof_Mismatch with
           "hace_cdr_buffer_t (" & Native_Size'Img &
           " bytes) no longer fits the Ada binding's opaque storage (" &
           Opaque_Size'Img &
           " bytes) -- update Opaque_Size in hace-cdr.ads";
      end if;

      Buf.Raw := (others => 0);
      Result :=
        To_Status
          (C_Buffer_Init
             (Buf.Raw'Address, Storage (Storage'First)'Address,
              Interfaces.C.size_t (Storage'Length), To_C (Order)));
   end Init;

   function Position (Buf : Buffer) return Natural is
   begin
      return Natural (C_Buffer_Position (Buf.Raw'Address));
   end Position;

   function Capacity (Buf : Buffer) return Natural is
   begin
      return Natural (C_Buffer_Capacity (Buf.Raw'Address));
   end Capacity;

   procedure Set_Position
     (Buf : in out Buffer; Position : Natural; Result : out Status)
   is
   begin
      Result :=
        To_Status
          (C_Buffer_Set_Position (Buf.Raw'Address, Interfaces.C.size_t (Position)));
   end Set_Position;

   procedure Encode_Octet (Buf : in out Buffer; Value : Octet; Result : out Status) is
   begin
      Result :=
        To_Status
          (C_Encode_Octet (Buf.Raw'Address, Interfaces.C.unsigned_char (Value)));
   end Encode_Octet;

   procedure Decode_Octet (Buf : in out Buffer; Value : out Octet; Result : out Status) is
      Raw : Interfaces.C.unsigned_char;
   begin
      Result := To_Status (C_Decode_Octet (Buf.Raw'Address, Raw));
      Value := Octet (Raw);
   end Decode_Octet;

   procedure Encode_Boolean (Buf : in out Buffer; Value : Boolean; Result : out Status) is
      C_Value : constant Interfaces.C.int := (if Value then 1 else 0);
   begin
      Result := To_Status (C_Encode_Bool (Buf.Raw'Address, C_Value));
   end Encode_Boolean;

   procedure Decode_Boolean (Buf : in out Buffer; Value : out Boolean; Result : out Status) is
      Raw : Interfaces.C.int;
   begin
      Result := To_Status (C_Decode_Bool (Buf.Raw'Address, Raw));
      Value := Raw /= 0;
   end Decode_Boolean;

   procedure Encode_U16
     (Buf : in out Buffer; Value : Interfaces.Unsigned_16; Result : out Status) is
   begin
      Result := To_Status (C_Encode_U16 (Buf.Raw'Address, Value));
   end Encode_U16;

   procedure Decode_U16
     (Buf : in out Buffer; Value : out Interfaces.Unsigned_16; Result : out Status) is
   begin
      Result := To_Status (C_Decode_U16 (Buf.Raw'Address, Value));
   end Decode_U16;

   procedure Encode_I16
     (Buf : in out Buffer; Value : Interfaces.Integer_16; Result : out Status) is
   begin
      Result := To_Status (C_Encode_I16 (Buf.Raw'Address, Value));
   end Encode_I16;

   procedure Decode_I16
     (Buf : in out Buffer; Value : out Interfaces.Integer_16; Result : out Status) is
   begin
      Result := To_Status (C_Decode_I16 (Buf.Raw'Address, Value));
   end Decode_I16;

   procedure Encode_U32
     (Buf : in out Buffer; Value : Interfaces.Unsigned_32; Result : out Status) is
   begin
      Result := To_Status (C_Encode_U32 (Buf.Raw'Address, Value));
   end Encode_U32;

   procedure Decode_U32
     (Buf : in out Buffer; Value : out Interfaces.Unsigned_32; Result : out Status) is
   begin
      Result := To_Status (C_Decode_U32 (Buf.Raw'Address, Value));
   end Decode_U32;

   procedure Encode_I32
     (Buf : in out Buffer; Value : Interfaces.Integer_32; Result : out Status) is
   begin
      Result := To_Status (C_Encode_I32 (Buf.Raw'Address, Value));
   end Encode_I32;

   procedure Decode_I32
     (Buf : in out Buffer; Value : out Interfaces.Integer_32; Result : out Status) is
   begin
      Result := To_Status (C_Decode_I32 (Buf.Raw'Address, Value));
   end Decode_I32;

   procedure Encode_U64
     (Buf : in out Buffer; Value : Interfaces.Unsigned_64; Result : out Status) is
   begin
      Result := To_Status (C_Encode_U64 (Buf.Raw'Address, Value));
   end Encode_U64;

   procedure Decode_U64
     (Buf : in out Buffer; Value : out Interfaces.Unsigned_64; Result : out Status) is
   begin
      Result := To_Status (C_Decode_U64 (Buf.Raw'Address, Value));
   end Decode_U64;

   procedure Encode_I64
     (Buf : in out Buffer; Value : Interfaces.Integer_64; Result : out Status) is
   begin
      Result := To_Status (C_Encode_I64 (Buf.Raw'Address, Value));
   end Encode_I64;

   procedure Decode_I64
     (Buf : in out Buffer; Value : out Interfaces.Integer_64; Result : out Status) is
   begin
      Result := To_Status (C_Decode_I64 (Buf.Raw'Address, Value));
   end Decode_I64;

   procedure Encode_F32
     (Buf : in out Buffer; Value : Interfaces.C.C_float; Result : out Status) is
   begin
      Result := To_Status (C_Encode_F32 (Buf.Raw'Address, Value));
   end Encode_F32;

   procedure Decode_F32
     (Buf : in out Buffer; Value : out Interfaces.C.C_float; Result : out Status) is
   begin
      Result := To_Status (C_Decode_F32 (Buf.Raw'Address, Value));
   end Decode_F32;

   procedure Encode_F64
     (Buf : in out Buffer; Value : Interfaces.C.double; Result : out Status) is
   begin
      Result := To_Status (C_Encode_F64 (Buf.Raw'Address, Value));
   end Encode_F64;

   procedure Decode_F64
     (Buf : in out Buffer; Value : out Interfaces.C.double; Result : out Status) is
   begin
      Result := To_Status (C_Decode_F64 (Buf.Raw'Address, Value));
   end Decode_F64;

end Hace.Cdr;
