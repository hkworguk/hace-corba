with Interfaces.C;

package body Hace.Corba_Ior is

   --  ---- Raw C struct mirror (runtime/include/hace/corba_ior.h) ----
   --
   --  Direct field-for-field mirror, not opaque -- plain data, no
   --  pointers, every field's natural C alignment checked by hand
   --  (host_ipv4 at offset 0, port at offset 4, object_key[32] at
   --  offset 6, 2 bytes of padding before object_key_length so it
   --  lands 8-byte aligned at offset 40, total size 48 -- already a
   --  multiple of 8, no trailing pad). Matches the Rust binding's
   --  identical by-hand check for hace_corba_ior_t (sys.rs).

   type Raw_Object_Key is
     array (0 .. Hace.Giop.Max_Object_Key_Length - 1) of Interfaces.Unsigned_8;
   pragma Convention (C, Raw_Object_Key);

   type Raw_Ior is record
      Host_Ipv4         : Interfaces.Unsigned_32 := 0;
      Port              : Interfaces.Unsigned_16 := 0;
      Object_Key        : Raw_Object_Key := (others => 0);
      Object_Key_Length : Interfaces.C.size_t := 0;
   end record;
   pragma Convention (C, Raw_Ior);

   --  ---- Raw C imports ----

   function C_Ior_Encode
     (Buf : System.Address; Ref : System.Address) return Interfaces.C.int;
   pragma Import (C, C_Ior_Encode, "hace_corba_ior_encode");

   function C_Ior_Decode
     (Buf : System.Address; Out_Ref : System.Address) return Interfaces.C.int;
   pragma Import (C, C_Ior_Decode, "hace_corba_ior_decode");

   function C_Object_To_String
     (Ref : System.Address; Out_Str : System.Address; Out_Capacity : Interfaces.C.size_t)
      return Interfaces.C.int;
   pragma Import (C, C_Object_To_String, "hace_corba_object_to_string");

   function C_String_To_Object
     (Str : System.Address; Out_Ref : System.Address) return Interfaces.C.int;
   pragma Import (C, C_String_To_Object, "hace_corba_string_to_object");

   --  ---- helpers ----

   function To_Status (Raw : Interfaces.C.int) return Status is
   begin
      case Raw is
         when 0 => return Ok;
         when 1 => return Err_Invalid_Argument;
         when 2 => return Err_Buffer_Overflow;
         when 3 => return Err_Buffer_Underflow;
         when others =>
            raise Program_Error with
              "hace_corba_status_t returned an unrecognized code" &
              Interfaces.C.int'Image (Raw) &
              " -- the Ada binding is out of sync with runtime/include/hace/corba_ior.h";
      end case;
   end To_Status;

   function To_Raw (Ref : Ior) return Raw_Ior is
      Result : Raw_Ior;
   begin
      Result.Host_Ipv4 := Interfaces.Unsigned_32 (Ref.Host_Addr);
      Result.Port := Ref.Port;
      for I in 0 .. Ref.Object_Key_Length - 1 loop
         Result.Object_Key (I) :=
           Interfaces.Unsigned_8 (Ref.Object_Key (Ref.Object_Key'First + I));
      end loop;
      Result.Object_Key_Length := Interfaces.C.size_t (Ref.Object_Key_Length);
      return Result;
   end To_Raw;

   function From_Raw (Raw : Raw_Ior) return Ior is
      Result : Ior;
   begin
      Result.Host_Addr := Hace.Hal.Ipv4_Address (Raw.Host_Ipv4);
      Result.Port := Raw.Port;
      Result.Object_Key_Length := Natural (Raw.Object_Key_Length);
      Result.Object_Key := (others => 0);
      for I in 0 .. Result.Object_Key_Length - 1 loop
         Result.Object_Key (Result.Object_Key'First + I) := Hace.Cdr.Octet (Raw.Object_Key (I));
      end loop;
      return Result;
   end From_Raw;

   Empty_Ior : constant Ior :=
     (Host_Addr => 0, Port => 0, Object_Key => (others => 0), Object_Key_Length => 0);

   --  ---- public subprograms ----

   procedure Encode (Buf : in out Hace.Cdr.Buffer; Ref : Ior; Result : out Status) is
      Raw : Raw_Ior := To_Raw (Ref);
   begin
      Result := To_Status (C_Ior_Encode (Hace.Cdr.Raw_Address (Buf), Raw'Address));
   end Encode;

   procedure Decode (Buf : in out Hace.Cdr.Buffer; Ref : out Ior; Result : out Status) is
      Raw : Raw_Ior;
   begin
      Result := To_Status (C_Ior_Decode (Hace.Cdr.Raw_Address (Buf), Raw'Address));
      if Result = Ok then
         Ref := From_Raw (Raw);
      else
         Ref := Empty_Ior;
      end if;
   end Decode;

   procedure Decode_At (Buf_Address : System.Address; Ref : out Ior; Result : out Status) is
      Raw : Raw_Ior;
   begin
      Result := To_Status (C_Ior_Decode (Buf_Address, Raw'Address));
      if Result = Ok then
         Ref := From_Raw (Raw);
      else
         Ref := Empty_Ior;
      end if;
   end Decode_At;

   procedure Encode_At (Buf_Address : System.Address; Ref : Ior; Result : out Status) is
      Raw : Raw_Ior := To_Raw (Ref);
   begin
      Result := To_Status (C_Ior_Encode (Buf_Address, Raw'Address));
   end Encode_At;

   procedure To_Uiop_String
     (Ref : Ior; Str : out String; Last : out Natural; Result : out Status)
   is
      Raw   : Raw_Ior := To_Raw (Ref);
      C_Out : Interfaces.C.char_array (0 .. Interfaces.C.size_t (Str'Length));
   begin
      Result :=
        To_Status
          (C_Object_To_String (Raw'Address, C_Out (C_Out'First)'Address, C_Out'Length));
      if Result = Ok then
         declare
            Decoded : constant String := Interfaces.C.To_Ada (C_Out, Trim_Nul => True);
         begin
            Last := Str'First + Decoded'Length - 1;
            Str (Str'First .. Last) := Decoded;
         end;
      else
         Last := Str'First - 1;
      end if;
   end To_Uiop_String;

   procedure From_Uiop_String (Str : String; Ref : out Ior; Result : out Status) is
      C_Str : constant Interfaces.C.char_array := Interfaces.C.To_C (Str);
      Raw   : Raw_Ior;
   begin
      Result := To_Status (C_String_To_Object (C_Str (C_Str'First)'Address, Raw'Address));
      if Result = Ok then
         Ref := From_Raw (Raw);
      else
         Ref := Empty_Ior;
      end if;
   end From_Uiop_String;

end Hace.Corba_Ior;
