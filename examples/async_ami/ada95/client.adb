--  async_ami client: sends a twoway Worker.compute(input) request via
--  Hace.Uiop.Send_Message directly (not the blocking Hace.Uiop.Call),
--  then polls for the reply with Hace.Uiop.Receive_Reply on a short
--  timeout in a loop, doing other visible work between polls -- the
--  Poller-style asynchronous invocation pattern, well suited to UDP's
--  non-blocking send (see docs/roadmap.md Phase 5). See
--  ../../client.idl for what this client calls.

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

   Async_Ami_Port : constant := 59121;
   Poll_Interval_Ms : constant := 50;
   Max_Polls : constant := 40; -- 40 * 50ms = 2s, comfortably more than the server's ~300ms
   Work_Object_Key : constant Hace.Cdr.Byte_Array (1 .. 4) :=
     (Hace.Cdr.Octet (Character'Pos ('w')), Hace.Cdr.Octet (Character'Pos ('o')),
      Hace.Cdr.Octet (Character'Pos ('r')), Hace.Cdr.Octet (Character'Pos ('k')));

   Server_Ior : constant Hace.Corba_Ior.Ior :=
     Example_Common.Parse_Ior_Arg (Async_Ami_Port, Work_Object_Key);

   Socket      : Hace.Hal.Socket_Handle;
   Started     : Boolean;
   T           : Hace.Uiop.Transport;

   Request_Id : constant := 1;
   Input      : constant := 6;

   Storage     : Hace.Cdr.Byte_Array (0 .. 255) := (others => 0);
   Request_Buf : Hace.Cdr.Buffer;
   Cdr_Result  : Hace.Cdr.Status;
   Giop_Result : Hace.Giop.Status;
   Uiop_Result : Hace.Uiop.Status;

   Reply_Body   : Hace.Cdr.Byte_Array (0 .. 255) := (others => 0);
   Reply_Last   : Integer;
   Is_Exception : Boolean;
   Got_Reply    : Boolean := False;

begin
   Example_Common.Open_Client_Socket ("async_ami client", Socket, Started);
   if not Started then
      return;
   end if;
   Hace.Uiop.For_Socket (T, Socket);

   Hace.Cdr.Init (Request_Buf, Storage, Hace.Cdr.Big_Endian, Cdr_Result);
   Hace.Giop.Begin_Message (Request_Buf, Hace.Cdr.Big_Endian, Giop_Result);
   if Giop_Result = Hace.Giop.Ok then
      Hace.Giop.Encode_Request_Header
        (Request_Buf, Request_Id, True, Work_Object_Key, "compute", Giop_Result);
   end if;
   if Giop_Result = Hace.Giop.Ok then
      Hace.Cdr.Encode_I32 (Request_Buf, Input, Cdr_Result);
      if Cdr_Result /= Hace.Cdr.Ok then
         Giop_Result := Hace.Giop.Err_Buffer_Overflow;
      end if;
   end if;
   if Giop_Result = Hace.Giop.Ok then
      Hace.Giop.Finish_Message (Request_Buf, Giop_Result);
   end if;
   if Giop_Result /= Hace.Giop.Ok then
      Put_Line (Standard_Error, "async_ami client: could not encode request");
      return;
   end if;

   Hace.Uiop.Send_Message
     (T, Request_Buf, Request_Id, Server_Ior.Host_Addr, Server_Ior.Port, Uiop_Result);
   if Uiop_Result /= Hace.Uiop.Ok then
      Put_Line (Standard_Error, "async_ami client: could not send request");
      return;
   end if;
   Put_Line ("client: sent compute(" & Integer'Image (Input) & " ), doing other work while waiting...");

   for Attempt in 1 .. Max_Polls loop
      Hace.Uiop.Receive_Reply
        (T, Request_Id, Poll_Interval_Ms, Reply_Body, Reply_Last, Is_Exception,
         Uiop_Result);
      if Uiop_Result = Hace.Uiop.Ok then
         Got_Reply := True;
         exit;
      elsif Uiop_Result = Hace.Uiop.Err_Timeout then
         Put_Line ("client: ...still working on other things (poll" & Attempt'Img & " )...");
      else
         Put_Line (Standard_Error, "async_ami client: poll error");
         return;
      end if;
   end loop;

   if not Got_Reply then
      Put_Line (Standard_Error, "async_ami client: never got a reply");
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
            "async_ami client: server raised exception " & Exc.Exception_Value'Img);
      end;
      return;
   end if;

   declare
      Result_Buf : Hace.Cdr.Buffer;
      Output     : Interfaces.Integer_32;
   begin
      Hace.Cdr.Init
        (Result_Buf, Reply_Body (Reply_Body'First .. Reply_Last), Hace.Cdr.Big_Endian,
         Cdr_Result);
      Hace.Cdr.Decode_I32 (Result_Buf, Output, Cdr_Result);
      if Cdr_Result = Hace.Cdr.Ok then
         Put_Line
           ("client: reply arrived -- compute(" & Integer'Image (Input) & " ) =" &
            Output'Img);
      else
         Put_Line (Standard_Error, "async_ami client: could not decode result");
      end if;
   end;
end Client;
