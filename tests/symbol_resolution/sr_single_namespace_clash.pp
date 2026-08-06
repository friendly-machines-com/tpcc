program sr_single_namespace_clash;

{ TARGET rule 1: types and values share one namespace per scope. A `type`
  declaration followed by a `var` of the same name in the same scope must be
  rejected as a duplicate. }

type
  T = Integer;

var
  T: Integer;

begin
end.
