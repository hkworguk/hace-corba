--  rpc_basic client: calls Calculator.add(a, b) via a blocking twoway
--  Hace.Uiop.Call and prints the result. Takes the server's IOR
--  string as an optional argv(1) (as printed by server.adb); defaults
--  to the localhost/Rpc_Basic_Port pair if not given.

with Ada.Text_IO; use Ada.Text_IO;
with Interfaces; use Interfaces;
with Hace.Cdr;
with Hace.Corba_Exceptions;
with Hace.Corba_Ior;
with Hace.Giop;
with Hace.Hal;
with Hace.Uiop;
with Example_Common;

use type Hace.Cdr.Status;
use type Hace.Giop.Status;
use type Hace.Uiop.Status;

procedure Client is

   Rpc_Basic_Port : constant := 59101;
   Calc_Object_Key : constant Hace.Cdr.Byte_Array (1 .. 4) :=
     (Hace.Cdr.Octet (Character'Pos ('c')), Hace.Cdr.Octet (Character'Pos ('a')),
      Hace.Cdr.Octet (Character'Pos ('l')), Hace.Cdr.Octet (Character'Pos ('c')));

   Server_Ior : constant Hace.Corba_Ior.Ior :=
     Example_Common.Parse_Ior_Arg (Rpc_Basic_Port, Calc_Object_Key);

   Socket      : Hace.Hal.Socket_Handle;
   Started     : Boolean;
   T           : Hace.Uiop.Transport;

   Storage     : Hace.Cdr.Byte_Array (0 .. 255) := (others => 0);
   Request_Buf : Hace.Cdr.Buffer;
   Cdr_Result  : Hace.Cdr.Status;
   Giop_Result : Hace.Giop.Status;

   A : constant := 7;
   B : constant := 35;

   Reply_Body      : Hace.Cdr.Byte_Array (0 .. 255) := (others => 0);
   Reply_Last      : Integer;
   Is_Exception    : Boolean;
   Uiop_Result     : Hace.Uiop.Status;

begin
   Example_Common.Open_Client_Socket ("rpc_basic client", Socket, Started);
   if not Started then
      return;
   end if;
   Hace.Uiop.For_Socket (T, Socket);

   Hace.Cdr.Init (Request_Buf, Storage, Hace.Cdr.Big_Endian, Cdr_Result);
   if Cdr_Result /= Hace.Cdr.Ok then
      Put_Line (Standard_Error, "rpc_basic client: could not init request buffer");
      return;
   end if;

   Hace.Giop.Begin_Message (Request_Buf, Hace.Cdr.Big_Endian, Giop_Result);
   if Giop_Result = Hace.Giop.Ok then
      Hace.Giop.Encode_Request_Header
        (Request_Buf, 1, True, Calc_Object_Key, "add", Giop_Result);
   end if;
   if Giop_Result = Hace.Giop.Ok then
      Hace.Cdr.Encode_I32 (Request_Buf, A, Cdr_Result);
      if Cdr_Result /= Hace.Cdr.Ok then
         Giop_Result := Hace.Giop.Err_Buffer_Overflow;
      end if;
   end if;
   if Giop_Result = Hace.Giop.Ok then
      Hace.Cdr.Encode_I32 (Request_Buf, B, Cdr_Result);
      if Cdr_Result /= Hace.Cdr.Ok then
         Giop_Result := Hace.Giop.Err_Buffer_Overflow;
      end if;
   end if;
   if Giop_Result = Hace.Giop.Ok then
      Hace.Giop.Finish_Message (Request_Buf, Giop_Result);
   end if;
   if Giop_Result /= Hace.Giop.Ok then
      Put_Line (Standard_Error, "rpc_basic client: could not encode request");
      return;
   end if;

   Hace.Uiop.Call
     (T, Request_Buf, 1, Server_Ior.Host_Addr, Server_Ior.Port, 1000, 3, Reply_Body,
      Reply_Last, Is_Exception, Uiop_Result);
   if Uiop_Result /= Hace.Uiop.Ok then
      Put_Line (Standard_Error, "rpc_basic client: call failed");
      return;
   end if;

   if Is_Exception then
      declare
         Exc_Buf    : Hace.Cdr.Buffer;
         Exc        : Hace.Corba_Exceptions.System_Exception;
         Exc_Result : Hace.Corba_Exceptions.Status;
      begin
         Hace.Cdr.Init
           (Exc_Buf, Reply_Body (Reply_Body'First .. Reply_Last), Hace.Cdr.Big_Endian,
            Cdr_Result);
         Hace.Corba_Exceptions.Decode_System_Exception (Exc_Buf, Exc, Exc_Result);
         Put_Line
           (Standard_Error,
            "rpc_basic client: server raised exception " & Exc.Exception_Value'Img);
      end;
      return;
   end if;

   declare
      Result_Buf : Hace.Cdr.Buffer;
      Sum        : Interfaces.Integer_32;
   begin
      Hace.Cdr.Init
        (Result_Buf, Reply_Body (Reply_Body'First .. Reply_Last), Hace.Cdr.Big_Endian,
         Cdr_Result);
      Hace.Cdr.Decode_I32 (Result_Buf, Sum, Cdr_Result);
      if Cdr_Result = Hace.Cdr.Ok then
         Put_Line ("client:" & Integer'Image (A) & " +" & Integer'Image (B) & " =" & Sum'Img);
      else
         Put_Line (Standard_Error, "rpc_basic client: could not decode result");
      end if;
   end;
end Client;
