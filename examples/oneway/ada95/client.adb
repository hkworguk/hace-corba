--  oneway client: sends three fire-and-forget Logger.log(message)
--  calls via Hace.Uiop.Send_Message directly -- no Hace.Uiop.Call, no
--  wait, no reply. Demonstrates genuine oneway semantics (a real
--  deviation from IIOP's reliable-transport-backed oneway -- see
--  docs/corba-spec-gap-analysis.md Sec 7). See ../../client.idl for
--  what this client calls.

with Ada.Text_IO; use Ada.Text_IO;
with Interfaces;
with Hace.Cdr;
with Hace.Corba_Ior;
with Hace.Giop;
with Hace.Hal;
with Hace.Uiop;
with Example_Common;

use type Hace.Cdr.Status;
use type Hace.Giop.Status;
use type Hace.Uiop.Status;

procedure Client is

   Oneway_Port : constant := 59111;
   Log_Object_Key : constant Hace.Cdr.Byte_Array (1 .. 3) :=
     (Hace.Cdr.Octet (Character'Pos ('l')), Hace.Cdr.Octet (Character'Pos ('o')),
      Hace.Cdr.Octet (Character'Pos ('g')));

   type Message_Array is array (Positive range <>) of access constant String;
   Messages : constant Message_Array :=
     (new String'("starting up"), new String'("processing batch 1"),
      new String'("all done"));

   Server_Ior : constant Hace.Corba_Ior.Ior :=
     Example_Common.Parse_Ior_Arg (Oneway_Port, Log_Object_Key);

   Socket  : Hace.Hal.Socket_Handle;
   Started : Boolean;
   T       : Hace.Uiop.Transport;

begin
   Example_Common.Open_Client_Socket ("oneway client", Socket, Started);
   if not Started then
      return;
   end if;
   Hace.Uiop.For_Socket (T, Socket);

   for I in Messages'Range loop
      declare
         Request_Id  : constant Interfaces.Unsigned_32 := Interfaces.Unsigned_32 (I);
         Storage     : Hace.Cdr.Byte_Array (0 .. 255) := (others => 0);
         Request_Buf : Hace.Cdr.Buffer;
         Cdr_Result  : Hace.Cdr.Status;
         Giop_Result : Hace.Giop.Status;
         Uiop_Result : Hace.Uiop.Status;
      begin
         Hace.Cdr.Init (Request_Buf, Storage, Hace.Cdr.Big_Endian, Cdr_Result);
         Hace.Giop.Begin_Message (Request_Buf, Hace.Cdr.Big_Endian, Giop_Result);
         if Giop_Result = Hace.Giop.Ok then
            Hace.Giop.Encode_Request_Header
              (Request_Buf, Request_Id, False, Log_Object_Key, "log", Giop_Result);
         end if;
         if Giop_Result = Hace.Giop.Ok then
            Hace.Cdr.Encode_String (Request_Buf, Messages (I).all, Cdr_Result);
            if Cdr_Result /= Hace.Cdr.Ok then
               Giop_Result := Hace.Giop.Err_Buffer_Overflow;
            end if;
         end if;
         if Giop_Result = Hace.Giop.Ok then
            Hace.Giop.Finish_Message (Request_Buf, Giop_Result);
         end if;
         if Giop_Result /= Hace.Giop.Ok then
            Put_Line (Standard_Error, "oneway client: could not encode message" & I'Img);
            goto Continue;
         end if;

         Hace.Uiop.Send_Message
           (T, Request_Buf, Request_Id, Server_Ior.Host_Addr, Server_Ior.Port,
            Uiop_Result);
         if Uiop_Result = Hace.Uiop.Ok then
            Put_Line ("client: sent log message" & I'Img & ": """ & Messages (I).all & """");
         else
            Put_Line (Standard_Error, "client: failed to send log message" & I'Img);
         end if;
      end;
      <<Continue>>
   end loop;
end Client;
