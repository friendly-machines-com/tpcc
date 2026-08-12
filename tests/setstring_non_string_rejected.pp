program SetStringNonStringRejected;

type
  TBytes = array[0..3] of Byte;
  TChars = array[0..3] of Char;

var
  Bytes: TBytes;
  Chars: TChars;

begin
  SetString(Bytes, @Chars[0], 4)
end.
