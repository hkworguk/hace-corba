with Interfaces.C;
with System;

package body Hace.Giop is

   use type Hace.Cdr.Byte_Order;

   --  ---- Raw C struct mirror (runtime/include/hace/giop.h) ----
   --
   --  Direct field-for-field mirror, not an opaque blob (contrast
   --  Hace.Cdr.Buffer/Hace.Corba_Poa.Poa): plain data, no pointers, no
   --  function pointers, and every field's natural C alignment checked
   --  by hand -- request_id (u32) and response_expected (int) are both
   --  4-byte aligned with no gap between them; object_key (uint8[32])
   --  needs no padding before or after it; object_key_length (size_t,
   --  8-byte aligned on LP64) lands at offset 40, already a multiple of
   --  8; operation (char[64]) follows at offset 48 with no further
   --  padding. Total size 112 bytes, already a multiple of 8 -- no
   --  trailing pad either. Matches the Rust binding's identical
   --  by-hand check for hace_giop_request_header_t (sys.rs).

   type Raw_Object_Key is array (0 .. Max_Object_Key_Length - 1) of Interfaces.Unsigned_8;
   pragma Convention (C, Raw_Object_Key);

   type Raw_Operation_Name is
     array (0 .. Max_Operation_Name_Length - 1) of Interfaces.C.char;
   pragma Convention (C, Raw_Operation_Name);

   type Raw_Request_Header is record
      Request_Id        : Interfaces.Unsigned_32 := 0;
      Response_Expected  : Interfaces.C.int := 0;
      Object_Key         : Raw_Object_Key := (others => 0);
      Object_Key_Length  : Interfaces.C.size_t := 0;
      Operation          : Raw_Operation_Name := (others => Interfaces.C.nul);
   end record;
   pragma Convention (C, Raw_Request_Header);

   --  ---- Raw C imports ----

   function C_Begin_Message
     (Buf : System.Address; Order : Interfaces.C.int) return Interfaces.C.int;
   pragma Import (C, C_Begin_Message, "hace_giop_begin_message");

   function C_Encode_Request_Header
     (Buf : System.Address; Header : System.Address) return Interfaces.C.int;
   pragma Import (C, C_Encode_Request_Header, "hace_giop_encode_request_header");

   function C_Finish_Message
     (Buf            : System.Address;
      Message_Type   : Interfaces.C.int;
      More_Fragments : Interfaces.C.int) return Interfaces.C.int;
   pragma Import (C, C_Finish_Message, "hace_giop_finish_message");

   --  ---- helpers ----

   function To_Status (Raw : Interfaces.C.int) return Status is
   begin
      case Raw is
         when 0 => return Ok;
         when 1 => return Err_Buffer_Overflow;
         when 2 => return Err_Buffer_Underflow;
         when 3 => return Err_Invalid_Argument;
         when 4 => return Err_Bad_Magic;
         when 5 => return Err_Unsupported_Version;
         when 6 => return Err_Unsupported_Message_Type;
         when 7 => return Err_Unsupported_Addressing_Disposition;
         when 8 => return Err_Unsupported_Sync_Scope;
         when 9 => return Err_Nonempty_Service_Context;
         when others =>
            raise Program_Error with
              "hace_giop_status_t returned an unrecognized code" &
              Interfaces.C.int'Image (Raw) &
              " -- the Ada binding is out of sync with runtime/include/hace/giop.h";
      end case;
   end To_Status;

   HACE_GIOP_MSG_REQUEST : constant Interfaces.C.int := 0;

   --  ---- public subprograms ----

   procedure Begin_Message
     (Buf : in out Hace.Cdr.Buffer; Order : Hace.Cdr.Byte_Order; Result : out Status)
   is
      C_Order : constant Interfaces.C.int :=
        (if Order = Hace.Cdr.Big_Endian then 0 else 1);
   begin
      Result := To_Status (C_Begin_Message (Hace.Cdr.Raw_Address (Buf), C_Order));
   end Begin_Message;

   procedure Encode_Request_Header
     (Buf               : in out Hace.Cdr.Buffer;
      Request_Id        : Interfaces.Unsigned_32;
      Response_Expected : Boolean;
      Object_Key        : Hace.Cdr.Byte_Array;
      Operation         : String;
      Result            : out Status)
   is
      Header : Raw_Request_Header;
   begin
      if Object_Key'Length > Max_Object_Key_Length then
         Result := Err_Invalid_Argument;
         return;
      end if;
      if Operation'Length >= Max_Operation_Name_Length then
         Result := Err_Invalid_Argument;
         return;
      end if;

      Header.Request_Id := Request_Id;
      Header.Response_Expected := (if Response_Expected then 1 else 0);
      for I in Object_Key'Range loop
         Header.Object_Key (I - Object_Key'First) := Interfaces.Unsigned_8 (Object_Key (I));
      end loop;
      Header.Object_Key_Length := Interfaces.C.size_t (Object_Key'Length);
      for I in Operation'Range loop
         Header.Operation (I - Operation'First) := Interfaces.C.To_C (Operation (I));
      end loop;

      Result :=
        To_Status
          (C_Encode_Request_Header (Hace.Cdr.Raw_Address (Buf), Header'Address));
   end Encode_Request_Header;

   procedure Finish_Message (Buf : in out Hace.Cdr.Buffer; Result : out Status) is
   begin
      Result :=
        To_Status
          (C_Finish_Message (Hace.Cdr.Raw_Address (Buf), HACE_GIOP_MSG_REQUEST, 0));
   end Finish_Message;

end Hace.Giop;
