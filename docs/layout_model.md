@page layout_model_page Positions, Groups & Layout

This page is the authoritative description of how StatusbarLog places bars on
the screen. The function-level documentation only summarises these rules and
links back here, so if anything elsewhere disagrees with this page, **this page
wins**.

@section lm_group Handles are groups

A @ref statusbar_log::StatusbarHandle "StatusbarHandle" does not refer to a
single bar. It refers to a _group_ of one or more bars that are created,
updated, and destroyed together.

A group owns a _contiguous block of terminal or file lines_. A group created with `N`
bars occupies exactly `N` adjacent lines: blank lines are never inserted
between the bars of one group, and the lines of two different groups are never
interleaved.

Concretely, the following hold for every group at all times:

- A group with `N` bars occupies exactly `N` lines.
- Those `N` lines are adjacent (no gaps between them).
- No other group's line ever appears between them.

@note A *bar* is one progress line such as
`prefix [####/     ]  50.00 postfix`. A _group_ is the set of bars behind one
handle. How bars are ordered within a group is covered in @ref lm_positions;
how groups are ordered relative to each other is covered in @ref lm_stacking.

@section lm_positions Relative position

As a @ref statusbar_log::StatusbarHandle "StatusbarHandle" with multiple bars (i.e.
a _group_ of bars) is created one assigns positions to every bar in the group. The
positional values function in the following way:

- positions are _ranks_ within the group (handle) only. Not a number of lines or
an offset value.
- Only the order of the position values matter, not the actual value. So `{5, 2}`
lays out identically to `{2, 1}`
- larger value = higher up / further away from the cursor

@note If the position values in every group aren't distinct an error will be thrown

@section lm_stacking How groups stack

@ref lm_positions describes the order of bars *inside* one group. This section
describes the order of whole groups *relative to each other*. You never assign a
group an absolute screen line; StatusbarLog derives it from **creation order**.

The rule is:

- The most recently created group sits **nearest the cursor** (at the bottom).
- Each older group floats **one block higher** than the group created after it.
- Equivalently: reading top-to-bottom, groups appear in the order they were
  created. Create `G1`, then `G2`, then `G3`, and the screen reads `G1` on top,
  `G3` just above the cursor.
- Because the caller never names an absolute line, two groups can never share one.

Each group still occupies its own contiguous block (see @ref lm_group), and the
bars inside that block are ordered by @ref lm_positions. Stacking only decides
the order of the blocks, never splits them.

For example, after creating `G1` (two bars, positions `{2, 1}`) and then `G2`
(one bar), the layout is:

@verbatim
  G1 bar @ position 2     <- top    (G1 is oldest, so highest)
  G1 bar @ position 1
  G2 bar                  <- bottom (G2 is newest, so nearest the cursor)
> _                                  the cursor / next prompt line
@endverbatim

@note Adding or removing a group therefore shifts where the other groups are
drawn. How and when the stack is repainted to reflect this is described in
@ref lm_reflow.

@section lm_reflow Redraw and reflow

StatusbarLog keeps the visible bars consistent with their stored state by
redrawing them at well-defined moments. Two terms are used:

- Repaint: Existing bars are redrawn, but the number of lines the stack
  occupies does not change.
- Reflow: the number of lines changes (a group is added or removed), so the
  absolute rows of other groups move and the whole stack is redrawn at its new
  positions.

The exact behaviour of each operation:

- @ref statusbar_log::UpdateStatusbar "UpdateStatusbar" repaints exactly one
  line: The single bar named by its index. No other bar is touched.
- A log call (@ref statusbar_log::Log "Log" and the `LogErr` / `LogWrn` /
  `LogInf` / `LogDbg` helpers) writes its message above the stack and then
  repaints **every** bar of the stack, so a log message never overwrites a bar.
- @ref statusbar_log::CreateStatusbarHandle "CreateStatusbarHandle" reflows:
  It opens the new group's lines nearest the cursor and redraws the whole stack.
- @ref statusbar_log::DestroyStatusbarHandle "DestroyStatusbarHandle"
  reflows: It removes the group's lines, closes the gap, and redraws the
  remaining stack.
