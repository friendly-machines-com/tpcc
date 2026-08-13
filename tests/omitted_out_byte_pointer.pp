program omitted_out_byte_pointer;

type
  TBytes = array[0..1] of Byte;
  TChars = array[0..1] of Char;
  PByte = ^Byte;
  PWord = ^Word;

procedure StoreByte(P: PByte; Value: Byte);
begin
  P^ := Value
end;

procedure Encode(out Buffer);
var
  P: PByte;
begin
  P := @Buffer;
  StoreByte(@Buffer, 42);
  Inc(P);
  P^ := 17
end;

procedure StoreAnsiChar(P: PAnsiChar; Value: AnsiChar);
begin
  P^ := Value
end;

procedure EncodeChars(out Buffer);
var
  P: PChar;
begin
  P := PChar(@Buffer);
  StoreAnsiChar(@Buffer, 'A');
  P := @Buffer;
  Inc(P);
  P^ := 'B'
end;

{$ifdef REJECT_WORD_POINTER}
procedure RejectWordPointer(out Buffer);
var
  P: PWord;
begin
  P := @Buffer
end;
{$endif}

{$ifdef REJECT_VAR_FORMAL}
procedure RejectVarFormal(var Buffer);
var
  P: PByte;
begin
  P := @Buffer
end;
{$endif}

{$ifdef REJECT_EXPLICIT_POINTER}
procedure RejectExplicitPointer(out Buffer);
var
  P: PWord;
begin
  P := PWord(@Buffer)
end;
{$endif}

var
  Bytes: TBytes;
  Chars: TChars;
begin
  Encode(Bytes);
  EncodeChars(Chars);
  WriteLn(Bytes[0]);
  WriteLn(Bytes[1]);
  WriteLn(Ord(Chars[0]));
  WriteLn(Ord(Chars[1]))
end.
