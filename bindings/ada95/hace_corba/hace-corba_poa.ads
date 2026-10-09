--  Ada95 bindings for the minimal Portable Object Adapter
--  (runtime/include/hace/corba_poa.h): static object activation only,
--  bounded (HACE_CORBA_POA_MAX_OBJECTS servants). See
--  docs/architecture/adr-0005-rust-ada-orb-bindings.md and
--  docs/architecture/adr-0004-corba-core.md.
--
--  hace_corba_poa_t is treated as opaque: it holds an array of entries
--  each containing a function pointer and a void* context, fragile to
--  mirror by hand and not worth the risk for a handle type nothing but
--  Init/Activate_Object/Handle_One_Request ever touches (same
--  rationale as Hace.Cdr.Buffer). Poa's Init verifies the opaque
--  storage is still large enough at runtime against
--  hace_corba_poa_sizeof(), the same way Hace.Cdr.Buffer's Init does
--  against hace_cdr_buffer_sizeof().

with Interfaces;
with Interfaces.C.Strings;
with System;
with Hace.Cdr;
with Hace.Corba_Exceptions;
with Hace.Uiop;

package Hace.Corba_Poa is

   type Status is
     (Ok,
      Err_Invalid_Argument,
      Err_Buffer_Overflow,
      Err_Buffer_Underflow);

   Poa_Sizeof_Mismatch : exception;

   type Poa is limited private;

   --  A servant: handles one operation on one activated object. Args
   --  is a bounded, read-only view of the decoded request's opaque
   --  argument bytes; Reply is caller-provided storage, positioned at
   --  0, for the servant to encode its result into. Both are
   --  System.Address, not Hace.Cdr.Buffer -- they point at a
   --  hace_cdr_buffer_t that corba_poa.c already built and owns, so a
   --  servant decodes/encodes through Hace.Cdr's _At subprograms (or
   --  Hace.Corba_Ior.Decode_At/Encode_At for an object reference
   --  argument), never by constructing its own Buffer. Out_Exception
   --  is zero-initialized (Exception_Value => None) by the dispatcher
   --  before the call; a servant only touches it to raise a system
   --  exception instead of returning a normal result, in which case
   --  whatever was written to Reply is ignored.
   --
   --  Declared at library level (not nested in a procedure) in every
   --  example, per Ada's standard constraint on subprograms used as C
   --  callbacks via 'Access.
   type Servant_Procedure is access procedure
     (Context       : System.Address;
      Operation     : Interfaces.C.Strings.chars_ptr;
      Args          : System.Address;
      Reply         : System.Address;
      Out_Exception : access Hace.Corba_Exceptions.System_Exception);
   pragma Convention (C, Servant_Procedure);

   procedure Init (P : out Poa; Result : out Status);

   --  Registers Servant/Context under Object_Key. Must be called
   --  before any call to Handle_One_Request on this Poa -- the servant
   --  table is read-only once dispatch begins.
   procedure Activate_Object
     (P          : in out Poa;
      Object_Key : Hace.Cdr.Byte_Array;
      Servant    : Servant_Procedure;
      Context    : System.Address;
      Result     : out Status);

   --  Waits up to Timeout_Ms for one Request, dispatches it to the
   --  matching activated servant, and sends a Reply -- unless the
   --  request was oneway, in which case nothing is sent back.
   --  Result = Hace.Uiop.Err_Timeout is routine ("no work this
   --  round"), not a failure -- matching
   --  hace_corba_poa_handle_one_request's own contract in C (note this
   --  is a Hace.Uiop.Status: dispatch mechanics, not the RPC outcome,
   --  which travels to the real client as the Reply's status).
   procedure Handle_One_Request
     (P          : Poa;
      T          : Hace.Uiop.Transport;
      Timeout_Ms : Interfaces.Unsigned_32;
      Result     : out Hace.Uiop.Status);

private

   Opaque_Size : constant := 768;
   --  Comfortably larger than sizeof(hace_corba_poa_t) (520 bytes on
   --  typical LP64 ABIs: 8 entries of 64 bytes each + an 8-byte count
   --  field); Init asserts this against hace_corba_poa_sizeof() rather
   --  than assuming it.

   type Opaque_Storage is array (1 .. Opaque_Size) of Interfaces.C.unsigned_char;
   pragma Convention (C, Opaque_Storage);

   type Poa is limited record
      Raw : Opaque_Storage := (others => 0);
   end record;

end Hace.Corba_Poa;
