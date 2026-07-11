program PackedTemporaryOverlay;

type
  TWordRec = packed record
    Hi, Lo: Byte;
  end;

function MakeWord: Word;
begin
  MakeWord := 0;
end;

begin
  TWordRec(MakeWord()).Hi := 1;
end.
