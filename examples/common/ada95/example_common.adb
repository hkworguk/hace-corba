with Ada.Command_Line;
with Ada.Text_IO; use Ada.Text_IO;

package body Example_Common is

   use type Hace.Hal.Status;
   use type Hace.Corba_Ior.Status;

   function Default_Ior
     (Port : Interfaces.Unsigned_16; Object_Key : Hace.Cdr.Byte_Array)
      return Hace.Corba_Ior.Ior
   is
      Ref : Hace.Corba_Ior.Ior;
   begin
      Ref.Host_Addr := Loopback;
      Ref.Port := Port;
      Ref.Object_Key := (others => 0);
      Ref.Object_Key (Ref.Object_Key'First .. Ref.Object_Key'First + Object_Key'Length - 1) :=
        Object_Key;
      Ref.Object_Key_Length := Object_Key'Length;
      return Ref;
   end Default_Ior;

   procedure Announce_Server (Role : String; Ref : Hace.Corba_Ior.Ior) is
      Str    : String (1 .. Hace.Corba_Ior.Max_Ior_String_Length);
      Last   : Natural;
      Result : Hace.Corba_Ior.Status;
   begin
      Hace.Corba_Ior.To_Uiop_String (Ref, Str, Last, Result);
      if Result = Hace.Corba_Ior.Ok then
         Put_Line (Role & " listening as " & Str (Str'First .. Last));
      else
         Put_Line (Standard_Error, Role & ": could not stringify its own IOR");
      end if;
   end Announce_Server;

   procedure Server_Start
     (Role       : String;
      Port       : Interfaces.Unsigned_16;
      Object_Key : Hace.Cdr.Byte_Array;
      Socket     : out Hace.Hal.Socket_Handle;
      Ref        : out Hace.Corba_Ior.Ior;
      Success    : out Boolean)
   is
      Hal_Result : Hace.Hal.Status;
   begin
      Hace.Hal.Open (Socket, Port, Hal_Result);
      if Hal_Result /= Hace.Hal.Ok then
         Put_Line
           (Standard_Error,
            Role & ": could not open UDP socket on port" & Port'Img);
         Success := False;
         return;
      end if;

      Ref := Default_Ior (Port, Object_Key);
      Announce_Server (Role, Ref);
      Success := True;
   end Server_Start;

   function Parse_Ior_Arg
     (Default_Port : Interfaces.Unsigned_16; Default_Object_Key : Hace.Cdr.Byte_Array)
      return Hace.Corba_Ior.Ior
   is
      Ref    : Hace.Corba_Ior.Ior;
      Result : Hace.Corba_Ior.Status;
   begin
      if Ada.Command_Line.Argument_Count >= 1 then
         Hace.Corba_Ior.From_Uiop_String (Ada.Command_Line.Argument (1), Ref, Result);
         if Result = Hace.Corba_Ior.Ok then
            return Ref;
         end if;
         Put_Line
           (Standard_Error,
            "warning: could not parse IOR argument '" & Ada.Command_Line.Argument (1) &
            "', using the localhost default instead");
      end if;
      return Default_Ior (Default_Port, Default_Object_Key);
   end Parse_Ior_Arg;

   procedure Open_Client_Socket
     (Role : String; Socket : out Hace.Hal.Socket_Handle; Success : out Boolean)
   is
      Hal_Result : Hace.Hal.Status;
   begin
      Hace.Hal.Open (Socket, 0, Hal_Result);
      if Hal_Result /= Hace.Hal.Ok then
         Put_Line (Standard_Error, Role & ": could not open UDP socket");
         Success := False;
         return;
      end if;
      Success := True;
   end Open_Client_Socket;

end Example_Common;
