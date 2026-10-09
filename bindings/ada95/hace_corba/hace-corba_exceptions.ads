--  Ada95 bindings for CORBA system exceptions
--  (runtime/include/hace/corba_exception.h): the direct-mirror
--  System_Exception record used both as a servant's out parameter
--  (via Hace.Corba_Poa.Servant_Procedure) and decoded from a Reply by
--  client code. See docs/architecture/adr-0005-rust-ada-orb-bindings.md
--  and docs/architecture/adr-0004-corba-core.md.

with Interfaces;
with Hace.Cdr;

package Hace.Corba_Exceptions is

   type Status is
     (Ok,
      Err_Invalid_Argument,
      Err_Buffer_Overflow,
      Err_Buffer_Underflow);

   --  A small, chosen subset of CORBA's standard system exceptions --
   --  enough to describe every failure this runtime can actually
   --  produce, not the full ~34-exception catalog.
   type Exception_Kind is
     (None, -- not an exception -- NO_EXCEPTION reply
      Unknown,
      Bad_Param,
      No_Memory,
      Object_Not_Exist,
      Bad_Operation,
      Transient,
      Timeout,
      Marshal,
      Comm_Failure);
   --  pragma Convention (C, ...) below is required, not decorative:
   --  without it GNAT would pick the smallest representation that
   --  fits this enum's literal count (a single byte) rather than a
   --  C int, breaking System_Exception's struct layout below.
   pragma Convention (C, Exception_Kind);

   --  Per the real CORBA SystemException convention: whether the
   --  operation is known to have executed, known not to have, or it's
   --  unknown -- useful for a caller deciding whether a retry is safe.
   type Completion_Status is (Completed_Yes, Completed_No, Completed_Maybe);
   pragma Convention (C, Completion_Status);

   --  Direct field-for-field mirror of hace_corba_system_exception_t
   --  (three plain enums/u32s, no pointers) -- this is the type a
   --  servant's Out_Exception parameter points at
   --  (Hace.Corba_Poa.Servant_Procedure), and corba_poa.c
   --  zero-initializes it (Exception => None) before every call.
   type System_Exception is record
      Exception_Value   : Exception_Kind := None;
      Minor_Code        : Interfaces.Unsigned_32 := 0;
      Completion        : Completion_Status := Completed_Yes;
   end record;
   pragma Convention (C, System_Exception);

   --  Decodes a Reply's exception body (the three-field wire format
   --  above) -- what client code calls after a twoway Call/
   --  Receive_Reply reports Is_Exception = True.
   procedure Decode_System_Exception
     (Buf : in out Hace.Cdr.Buffer; Exc : out System_Exception; Result : out Status);

end Hace.Corba_Exceptions;
