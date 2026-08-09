program VariantConstantOverlap;

type
  TOverlay = packed record
    case Boolean of
      False: (A, B: Byte);
      True: (C, D: Byte)
  end;

const
  Value: TOverlay = (A: 1; C: 2);

begin
end.
