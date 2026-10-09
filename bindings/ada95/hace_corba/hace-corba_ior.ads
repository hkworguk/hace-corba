--  Ada95 bindings for CORBA object references (IOR)
--  (runtime/include/hace/corba_ior.h): a UIOP profile (host, UDP port,
--  object key) rather than IIOP, with corbaloc-style
--  "uiop:host:port/hexkey" stringification. See
--  docs/architecture/adr-0005-rust-ada-orb-bindings.md and
--  docs/architecture/adr-0004-corba-core.md.

with Interfaces;
with System;
with Hace.Cdr;
with Hace.Giop;
with Hace.Hal;

package Hace.Corba_Ior is

   Max_Ior_String_Length : constant := 96;

   type Status is
     (Ok,
      Err_Invalid_Argument,
      Err_Buffer_Overflow,
      Err_Buffer_Underflow);

   type Ior is record
      Host_Addr         : Hace.Hal.Ipv4_Address;
      Port              : Interfaces.Unsigned_16;
      Object_Key        : Hace.Cdr.Byte_Array (1 .. Hace.Giop.Max_Object_Key_Length);
      Object_Key_Length : Natural;
   end record;

   --  CDR-encodes/decodes Ref as a request/reply argument (e.g.
   --  observer_attach's attach(in Observer obs)).
   procedure Encode (Buf : in out Hace.Cdr.Buffer; Ref : Ior; Result : out Status);
   procedure Decode (Buf : in out Hace.Cdr.Buffer; Ref : out Ior; Result : out Status);

   --  Address-based twins of Encode/Decode, for a servant procedure's
   --  Args/Reply parameters (System.Address pointing at a
   --  hace_cdr_buffer_t that hace_corba_poa.c already built and owns
   --  -- see Hace.Cdr's own _At subprograms for why). Used by
   --  observer_attach's subject_server, whose attach servant decodes
   --  the caller's Observer reference straight out of Args.
   procedure Decode_At (Buf_Address : System.Address; Ref : out Ior; Result : out Status);
   procedure Encode_At (Buf_Address : System.Address; Ref : Ior; Result : out Status);

   --  "uiop:a.b.c.d:port/hexkey", as a server announces its own
   --  address on startup.
   procedure To_Uiop_String (Ref : Ior; Str : out String; Last : out Natural; Result : out Status);

   --  Parses a string in exactly the format To_Uiop_String produces.
   procedure From_Uiop_String (Str : String; Ref : out Ior; Result : out Status);

end Hace.Corba_Ior;
