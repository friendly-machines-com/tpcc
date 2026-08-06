program sr_class_field_as_type;

{ TARGET: ClassType.class_var_field used as a type argument. }

{$mode delphi}

type
  TClass = class
    class var F: Integer;
  end;

var
  L: 0..High(TClass.F);

begin
  L := Low(TClass.F);
  if L <> 0 then Halt(1)
end.
