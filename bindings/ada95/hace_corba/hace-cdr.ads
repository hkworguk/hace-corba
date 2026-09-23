--  Ada95 bindings for the hace-corba CDR encode/decode core
--  (runtime/include/hace/cdr.h), following the encode/decode/status
--  conventions of the OMG IDL to Ada Language Mapping referenced in
--  docs/standards/spec-references.md, adapted for hand-binding directly
--  onto the runtime rather than generated from IDL (see
--  docs/roadmap.md -- the Ada95 idlc backend is Phase 4, not built yet).
--
--  hace_cdr_buffer_t is treated as opaque: rather than mirroring its C
--  field layout (which risks silently drifting out of sync as the
--  struct evolves), Buffer wraps a fixed byte array sized generously
--  larger than the real struct; Init verifies that at runtime against
--  hace_cdr_buffer_sizeof() and raises Buffer_Sizeof_Mismatch if it no
--  longer fits (mirrors the assertion in the Rust binding's
--  CdrBuffer::new, bindings/rust/hace-corba/src/cdr.rs).
--
--  Status is returned via an `out` parameter rather than an exception
--  for ordinary encode/decode failures (buffer overflow/underflow,
--  invalid argument), matching hace_cdr_status_t's no-exceptions design
--  in the C runtime.

with Interfaces;
with Interfaces.C;

package Hace.Cdr is

   type Byte_Order is (Big_Endian, Little_Endian);

   type Status is
     (Ok,
      Err_Buffer_Overflow,
      Err_Buffer_Underflow,
      Err_Invalid_Argument);

   type Octet is new Interfaces.Unsigned_8;
   type Byte_Array is array (Natural range <>) of Octet;

   Buffer_Sizeof_Mismatch : exception;

   type Buffer is limited private;

   --  Binds Buf to Storage for Buf's lifetime. Storage must remain live
   --  and untouched by other code for as long as Buf is used.
   procedure Init
     (Buf     : out Buffer;
      Storage : in out Byte_Array;
      Order   : Byte_Order;
      Result  : out Status);

   --  Bytes produced so far (encode) or the read cursor (decode).
   function Position (Buf : Buffer) return Natural;
   function Capacity (Buf : Buffer) return Natural;

   --  Rewinds/seeks the cursor -- e.g. Set_Position (Buf, 0, Result)
   --  before decoding what was just encoded into the same buffer.
   procedure Set_Position
     (Buf      : in out Buffer;
      Position : Natural;
      Result   : out Status);

   procedure Encode_Octet (Buf : in out Buffer; Value : Octet; Result : out Status);
   procedure Decode_Octet (Buf : in out Buffer; Value : out Octet; Result : out Status);

   procedure Encode_Boolean (Buf : in out Buffer; Value : Boolean; Result : out Status);
   procedure Decode_Boolean (Buf : in out Buffer; Value : out Boolean; Result : out Status);

   procedure Encode_U16
     (Buf : in out Buffer; Value : Interfaces.Unsigned_16; Result : out Status);
   procedure Decode_U16
     (Buf : in out Buffer; Value : out Interfaces.Unsigned_16; Result : out Status);

   procedure Encode_I16
     (Buf : in out Buffer; Value : Interfaces.Integer_16; Result : out Status);
   procedure Decode_I16
     (Buf : in out Buffer; Value : out Interfaces.Integer_16; Result : out Status);

   procedure Encode_U32
     (Buf : in out Buffer; Value : Interfaces.Unsigned_32; Result : out Status);
   procedure Decode_U32
     (Buf : in out Buffer; Value : out Interfaces.Unsigned_32; Result : out Status);

   procedure Encode_I32
     (Buf : in out Buffer; Value : Interfaces.Integer_32; Result : out Status);
   procedure Decode_I32
     (Buf : in out Buffer; Value : out Interfaces.Integer_32; Result : out Status);

   procedure Encode_U64
     (Buf : in out Buffer; Value : Interfaces.Unsigned_64; Result : out Status);
   procedure Decode_U64
     (Buf : in out Buffer; Value : out Interfaces.Unsigned_64; Result : out Status);

   procedure Encode_I64
     (Buf : in out Buffer; Value : Interfaces.Integer_64; Result : out Status);
   procedure Decode_I64
     (Buf : in out Buffer; Value : out Interfaces.Integer_64; Result : out Status);

   procedure Encode_F32
     (Buf : in out Buffer; Value : Interfaces.C.C_float; Result : out Status);
   procedure Decode_F32
     (Buf : in out Buffer; Value : out Interfaces.C.C_float; Result : out Status);

   procedure Encode_F64
     (Buf : in out Buffer; Value : Interfaces.C.double; Result : out Status);
   procedure Decode_F64
     (Buf : in out Buffer; Value : out Interfaces.C.double; Result : out Status);

private

   Opaque_Size : constant := 64;
   --  Comfortably larger than sizeof(hace_cdr_buffer_t) (32 bytes on
   --  typical LP64 ABIs); Init asserts this against
   --  hace_cdr_buffer_sizeof() rather than assuming it.

   type Opaque_Storage is array (1 .. Opaque_Size) of Interfaces.C.unsigned_char;
   pragma Convention (C, Opaque_Storage);

   type Buffer is limited record
      Raw : Opaque_Storage := (others => 0);
   end record;

end Hace.Cdr;
