package body Hace.Uiop is

   use type Interfaces.C.size_t;

   HACE_GIOP_MSG_REQUEST : constant Interfaces.C.int := 0;
   HACE_GIOP_MSG_REPLY   : constant Interfaces.C.int := 1;

   --  ---- Raw C struct mirrors, body-local (no application code needs
   --  these directly; Receive_Request/Receive_Reply convert into the
   --  public Request_Info/plain-parameter shapes above) ----

   type Raw_Message_Header is record
      Version_Major  : Interfaces.Unsigned_8 := 0;
      Version_Minor  : Interfaces.Unsigned_8 := 0;
      Byte_Order     : Interfaces.C.int := 0;
      More_Fragments : Interfaces.C.int := 0;
      Message_Type   : Interfaces.C.int := 0;
      Message_Size   : Interfaces.Unsigned_32 := 0;
   end record;
   pragma Convention (C, Raw_Message_Header);

   type Raw_Object_Key is
     array (0 .. Hace.Giop.Max_Object_Key_Length - 1) of Interfaces.Unsigned_8;
   pragma Convention (C, Raw_Object_Key);

   subtype Raw_Operation_Name is
     Interfaces.C.char_array (0 .. Hace.Giop.Max_Operation_Name_Length - 1);

   type Raw_Request_Info is record
      Object_Key        : Raw_Object_Key := (others => 0);
      Object_Key_Length : Interfaces.C.size_t := 0;
      Operation         : Raw_Operation_Name := (others => Interfaces.C.nul);
      Response_Expected : Interfaces.C.int := 0;
   end record;
   pragma Convention (C, Raw_Request_Info);

   --  ---- Raw C imports (runtime/include/hace/uiop.h) ----

   function C_Transport_For_Socket (Handle : System.Address) return Raw_Transport;
   pragma Import (C, C_Transport_For_Socket, "hace_uiop_transport_for_socket");

   function C_Send_Message
     (Transport_Addr : System.Address;
      Message        : System.Address;
      Message_Type   : Interfaces.C.int;
      Request_Id     : Interfaces.Unsigned_32;
      Dest_Addr      : Interfaces.Unsigned_32;
      Dest_Port      : Interfaces.C.unsigned_short;
      Max_Datagram   : Interfaces.C.size_t) return Interfaces.C.int;
   pragma Import (C, C_Send_Message, "hace_uiop_send_message");

   function C_Receive
     (Transport_Addr   : System.Address;
      Expected_Type    : Interfaces.C.int;
      Match_Request_Id : System.Address;
      Timeout_Ms       : Interfaces.Unsigned_32;
      Body_Storage     : System.Address;
      Body_Capacity    : Interfaces.C.size_t;
      Out_Header       : System.Address;
      Out_Request_Id   : out Interfaces.Unsigned_32;
      Out_Reply_Status : System.Address;
      Out_Body_Length  : out Interfaces.C.size_t;
      Out_Src_Addr     : System.Address;
      Out_Src_Port     : System.Address;
      Out_Request_Info : System.Address) return Interfaces.C.int;
   pragma Import (C, C_Receive, "hace_uiop_receive");

   function C_Call
     (Transport_Addr         : System.Address;
      Request                : System.Address;
      Request_Id             : Interfaces.Unsigned_32;
      Dest_Addr              : Interfaces.Unsigned_32;
      Dest_Port              : Interfaces.C.unsigned_short;
      Max_Datagram           : Interfaces.C.size_t;
      Per_Attempt_Timeout_Ms : Interfaces.Unsigned_32;
      Max_Attempts           : Interfaces.Unsigned_32;
      Reply_Body_Storage     : System.Address;
      Reply_Body_Capacity    : Interfaces.C.size_t;
      Out_Reply_Status       : out Interfaces.C.int;
      Out_Reply_Body_Length  : out Interfaces.C.size_t) return Interfaces.C.int;
   pragma Import (C, C_Call, "hace_uiop_call");

   --  ---- helpers ----

   function To_Status (Raw : Interfaces.C.int) return Status is
   begin
      case Raw is
         when 0 => return Ok;
         when 1 => return Err_Invalid_Argument;
         when 2 => return Err_Hal;
         when 3 => return Err_Too_Large;
         when 4 => return Err_Timeout;
         when others =>
            raise Program_Error with
              "hace_uiop_status_t returned an unrecognized code" &
              Interfaces.C.int'Image (Raw) &
              " -- the Ada binding is out of sync with runtime/include/hace/uiop.h";
      end case;
   end To_Status;

   --  ---- public subprograms ----

   procedure For_Socket (T : out Transport; Handle : Hace.Hal.Socket_Handle) is
   begin
      T.Handle_Copy := Hace.Hal.To_Raw (Handle);
      T.Raw := C_Transport_For_Socket (T.Handle_Copy'Address);
   end For_Socket;

   procedure Send_Message
     (T          : Transport;
      Message    : in out Hace.Cdr.Buffer;
      Request_Id : Interfaces.Unsigned_32;
      Dest_Addr  : Hace.Hal.Ipv4_Address;
      Dest_Port  : Interfaces.Unsigned_16;
      Result     : out Status)
   is
   begin
      Result :=
        To_Status
          (C_Send_Message
             (T.Raw'Address, Hace.Cdr.Raw_Address (Message), HACE_GIOP_MSG_REQUEST,
              Request_Id, Interfaces.Unsigned_32 (Dest_Addr),
              Interfaces.C.unsigned_short (Dest_Port), Max_Datagram_Size));
   end Send_Message;

   procedure Receive_Request
     (T           : Transport;
      Timeout_Ms  : Interfaces.Unsigned_32;
      Body_Data   : out Hace.Cdr.Byte_Array;
      Body_Last   : out Integer;
      Request_Id  : out Interfaces.Unsigned_32;
      Src_Addr    : out Hace.Hal.Ipv4_Address;
      Src_Port    : out Interfaces.Unsigned_16;
      Info        : out Request_Info;
      Result      : out Status)
   is
      Header       : Raw_Message_Header;
      Body_Length  : Interfaces.C.size_t := 0;
      Raw_Src_Addr : aliased Interfaces.Unsigned_32 := 0;
      Raw_Src_Port : aliased Interfaces.C.unsigned_short := 0;
      Raw_Info     : aliased Raw_Request_Info;
   begin
      Request_Id := 0;
      Result :=
        To_Status
          (C_Receive
             (T.Raw'Address, HACE_GIOP_MSG_REQUEST, System.Null_Address, Timeout_Ms,
              Body_Data (Body_Data'First)'Address,
              Interfaces.C.size_t (Body_Data'Length), Header'Address, Request_Id,
              System.Null_Address, Body_Length, Raw_Src_Addr'Address,
              Raw_Src_Port'Address, Raw_Info'Address));

      if Result = Ok then
         Body_Last := Body_Data'First + Integer (Body_Length) - 1;
         Src_Addr := Hace.Hal.Ipv4_Address (Raw_Src_Addr);
         Src_Port := Interfaces.Unsigned_16 (Raw_Src_Port);

         Info.Object_Key_Length := Natural (Raw_Info.Object_Key_Length);
         for I in 0 .. Info.Object_Key_Length - 1 loop
            Info.Object_Key (Info.Object_Key'First + I) :=
              Hace.Cdr.Octet (Raw_Info.Object_Key (I));
         end loop;

         declare
            Op_Str : constant String :=
              Interfaces.C.To_Ada (Raw_Info.Operation, Trim_Nul => True);
         begin
            Info.Operation (Info.Operation'First .. Info.Operation'First + Op_Str'Length - 1) :=
              Op_Str;
            Info.Operation_Last := Info.Operation'First + Op_Str'Length - 1;
         end;
         Info.Response_Expected := Raw_Info.Response_Expected /= 0;
      else
         Body_Last := Body_Data'First - 1;
         Info.Object_Key_Length := 0;
         Info.Operation_Last := Info.Operation'First - 1;
         Info.Response_Expected := False;
      end if;
   end Receive_Request;

   procedure Receive_Reply
     (T                : Transport;
      Match_Request_Id : Interfaces.Unsigned_32;
      Timeout_Ms       : Interfaces.Unsigned_32;
      Body_Data        : out Hace.Cdr.Byte_Array;
      Body_Last        : out Integer;
      Is_Exception     : out Boolean;
      Result           : out Status)
   is
      Header           : Raw_Message_Header;
      Out_Request_Id   : Interfaces.Unsigned_32 := 0;
      Reply_Status     : aliased Interfaces.C.int := 0;
      Body_Length      : Interfaces.C.size_t := 0;
      Match_Id_Storage : aliased Interfaces.Unsigned_32 := Match_Request_Id;
   begin
      Result :=
        To_Status
          (C_Receive
             (T.Raw'Address, HACE_GIOP_MSG_REPLY, Match_Id_Storage'Address, Timeout_Ms,
              Body_Data (Body_Data'First)'Address,
              Interfaces.C.size_t (Body_Data'Length), Header'Address, Out_Request_Id,
              Reply_Status'Address, Body_Length, System.Null_Address,
              System.Null_Address, System.Null_Address));

      if Result = Ok then
         Body_Last := Body_Data'First + Integer (Body_Length) - 1;
         Is_Exception := Reply_Status /= 0;
      else
         Body_Last := Body_Data'First - 1;
         Is_Exception := False;
      end if;
   end Receive_Reply;

   function Raw_Address (T : Transport) return System.Address is
   begin
      return T.Raw'Address;
   end Raw_Address;

   procedure Call
     (T                      : Transport;
      Request                : in out Hace.Cdr.Buffer;
      Request_Id             : Interfaces.Unsigned_32;
      Dest_Addr              : Hace.Hal.Ipv4_Address;
      Dest_Port              : Interfaces.Unsigned_16;
      Per_Attempt_Timeout_Ms : Interfaces.Unsigned_32;
      Max_Attempts           : Interfaces.Unsigned_32;
      Reply_Body             : out Hace.Cdr.Byte_Array;
      Reply_Body_Last        : out Integer;
      Is_Exception           : out Boolean;
      Result                 : out Status)
   is
      Reply_Status      : Interfaces.C.int := 0;
      Reply_Body_Length : Interfaces.C.size_t := 0;
   begin
      Result :=
        To_Status
          (C_Call
             (T.Raw'Address, Hace.Cdr.Raw_Address (Request), Request_Id,
              Interfaces.Unsigned_32 (Dest_Addr), Interfaces.C.unsigned_short (Dest_Port),
              Max_Datagram_Size, Per_Attempt_Timeout_Ms, Max_Attempts,
              Reply_Body (Reply_Body'First)'Address,
              Interfaces.C.size_t (Reply_Body'Length), Reply_Status, Reply_Body_Length));

      if Result = Ok then
         Reply_Body_Last := Reply_Body'First + Integer (Reply_Body_Length) - 1;
         Is_Exception := Reply_Status /= 0;
      else
         Reply_Body_Last := Reply_Body'First - 1;
         Is_Exception := False;
      end if;
   end Call;

end Hace.Uiop;
