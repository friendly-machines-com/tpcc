program sr_chained_record_field_as_type;

{ TARGET: chained `Low(Outer.Inner.I)`--recursive application of the
  RecordType.field-as-type rule across multiple levels. }

{$mode delphi}

type
  TInner = record
    I: Integer;
  end;
  TOuter = record
    Inner: TInner;
  end;

var
  L: 0..High(TOuter.Inner.I);

begin
  L := Low(TOuter.Inner.I);
  if L <> 0 then Halt(1)
end.
