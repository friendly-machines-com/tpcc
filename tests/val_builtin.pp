program ValBuiltin;

type
  TSignedRange = -5..5;
  TUnsignedRange = 0..255;

var
  Code: Integer;
  LongCode: LongInt;
  ByteCode: Byte;
  SI8: ShortInt;
  SI16: SmallInt;
  SI32: LongInt;
  SI64: Int64;
  UI8: Byte;
  UI16: Word;
  UI32: LongWord;
  UI64: QWord;
  SR: TSignedRange;
  UR: TUnsignedRange;
  D: Double;
  E: Extended;

begin
  Val('-8', SI8, Code);
  Val('-1600', SI16, Code);
  Val('-320000', SI32, Code);
  Val('-320000', SI32, LongCode);
  Val('12x', SI32, ByteCode);
  Val('-640000', SI64, Code);
  Val('8', UI8, Code);
  Val('1600', UI16, Code);
  Val('320000', UI32, Code);
  Val('640000', UI64, Code);
  Val('-4', SR, Code);
  Val('200', UR, Code);
  Val('1.25', D, Code);
  Val('-2.5e2', E, Code);

  Val('7', SI32);
  Val('8', UI32);
  Val('3.5', D)
end.
