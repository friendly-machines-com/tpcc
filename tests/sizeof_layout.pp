program SizeOfLayout;

type
  TIntegerBytes = array[1..SizeOf(Integer)] of Byte;
  TPadded = record
    First: Byte;
    Middle: Integer;
    Last: Word;
  end;
  TVarSectionPadded = record
  var
    Zed: Byte;
    Alpha: Integer;
    Omega: Word;
  end;
  TVariant = record
    Prefix: Byte;
    case Byte of
      0: (Small: Byte; Number: Integer);
      1: (Large: QWord);
  end;
  TPacked = packed record
    First: Byte;
    Middle: Integer;
    Last: Word;
  end;
  TShortStringRecord = record
    Name: string[2];
    Enabled: Boolean;
    Define: string[5];
  end;

const
  IntegerSize: SizeInt = SizeOf(Integer);

var
  Bytes: TIntegerBytes;
  Padded: TPadded;
  Variant: TVariant;
  PackedValue: TPacked;
  ShortName: string[2];
  ShortStringRecord: TShortStringRecord;
  N: SizeInt;

begin
  N := SizeOf(Bytes);
  N := SizeOf(TIntegerBytes);
  N := SizeOf(Padded);
  N := SizeOf(TPadded);
  N := SizeOf(Variant);
  N := SizeOf(TVariant);
  N := SizeOf(PackedValue);
  N := SizeOf(TPacked);
  N := SizeOf(ShortName);
  N := SizeOf(ShortStringRecord);
  N := SizeOf(TShortStringRecord);
  Variant.Small := 1;
  Variant.Number := 2;
  Variant.Large := 3
end.
