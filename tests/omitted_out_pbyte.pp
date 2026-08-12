program omitted_out_pbyte;

type
  TBytes = array[0..1] of Byte;
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
begin
  Encode(Bytes);
  WriteLn(Bytes[0]);
  WriteLn(Bytes[1])
end.
