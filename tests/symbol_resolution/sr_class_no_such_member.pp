program sr_class_no_such_member;

{ TARGET: ClassType.NoSuchMember as a type argument--RHS not in the
  class's frame. }

{$mode delphi}

type
  TClass = class
    class var F: Integer;
  end;

var
  L: 0..High(TClass.NoSuchMember);

begin
end.
