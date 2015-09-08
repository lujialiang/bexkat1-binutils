/* tc-bexkat1.c -- Assembler code for the Bexkat 1.
   Written by Matt Stock (stock@csgeeks.org)
 */

#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "opcode/bexkat1.h"

const char comment_chars[] = "#";
const char line_comment_chars[] = "#";
const char line_separator_chars[] = ";";

const char EXP_CHARS[] = "eE";
const char FLT_CHARS[] = "rRsSfFdDxXpP";

static int pending_reloc;
static struct hash_control *opcode_hash_control;

extern int target_big_endian;

static valueT md_chars_to_number (char * buf, int n);

const pseudo_typeS md_pseudo_table[] =
  {
    { 0, 0, 0}
  };

void
md_operand (expressionS *op __attribute__((unused)))
{
}

void
md_begin (void)
{
  int count;
  const bexkat1_opc_info_t *opcode;
  opcode_hash_control = hash_new();

  bfd_set_arch_mach(stdoutput, TARGET_ARCH, 0);

  opcode = bexkat1_opc_info;
  for (count=0; count < bexkat1_opc_count; count++) {
    hash_insert(opcode_hash_control, opcode->name, (char *)opcode);
    opcode++;
  }
}

static char *
parse_exp_save_ilp (char *s, expressionS *op)
{
  char *save = input_line_pointer;
  
  input_line_pointer = s;
  expression(op);
  s = input_line_pointer;
  input_line_pointer = save;
  return s;
}

static int
parse_regnum(char **ptr)
{
  int reg;
  char *s = *ptr;

  if (*s != '%') {
    as_bad(_("not a register: %s"), s);
    return -1;
  }
  s++;

  // %fp alias for %14
  if (s[0] == 'f' && s[1] == 'p') {
    *ptr += 3;
    return 14;
  }

  // %sp alias for %15
  if (s[0] == 's' && s[1] == 'p') {
    *ptr += 3;
    return 15;
  }

  reg = *s - '0';
  if ((reg < 0) || (reg > 9)) {
    as_bad(_("illegal register number 1 %s"), s);
    ignore_rest_of_line();
    return -1;
  }
  s++;

  if ((reg > 0) || (reg < 4)) {
    int r2 = *s - '0';
    if ((r2 >= 0) && (r2 <= 9)) {
      reg = reg*10 + r2;
      if (reg > 31) {
	as_bad(_("illegal register number %d"), reg);
	ignore_rest_of_line();
	return -1;
      }
      *ptr += 1;
    }
  } 
  
  *ptr += 2;
  return reg;
}

/* Convert the instructions into frags and bytes */
void
md_assemble(char *str)
{
  char *op_start;
  char *op_end;
  char op_name[10];
  const bexkat1_opc_info_t *opcode;
  char *p;
  char pend;
  unsigned int iword;
  int nlen = 0;
  expressionS arg;
  int regnum;
  int offset;
  
  while (*str == ' ')
    str++;
  
  // mark opcode
  op_start = str;
  for (op_end = str;
       *op_end && !is_end_of_line[*op_end & 0xff] && *op_end != ' ';
       op_end++)
    nlen++;
  
  pend = *op_end;
  *op_end = 0;
  strncpy(op_name, op_start, 10);
  *op_end = pend;

  if (nlen == 0)
    as_bad(_("can't find opcode "));
  
  while (ISSPACE(*op_end))
    op_end++;
  
  opcode = (bexkat1_opc_info_t *) hash_find(opcode_hash_control, op_name);
  if (opcode == NULL) {
    as_bad(_("unknown opcode %s"), op_name);
    return;
  }

  switch (opcode->mode) {
  case BEXKAT1_REG:
    iword = (BEXKAT1_REG << 30) | (opcode->opcode << 23);
    if (opcode->args > 0) {
      regnum = parse_regnum(&op_end);
      if (regnum == -1)
	return; 
      while (ISSPACE(*op_end))
	op_end++;
      iword |= (regnum & 0xf) << 16;
    }
    if (opcode->args > 1) {
      if (*op_end != ',') {
	as_bad(_("missing comma: %s"), op_end);
	return;
      }
      op_end++;
      while (ISSPACE(*op_end))
	op_end++;
      regnum = parse_regnum(&op_end);
      if (regnum == -1)
	return;
      iword |= ((regnum & 0xf) << 12);
    }
    if (opcode->args > 2) {
      if (*op_end != ',') {
	as_bad(_("missing comma: %s"), op_end);
	return;
      }
      op_end++;
      while (ISSPACE(*op_end))
	op_end++;
      regnum = parse_regnum(&op_end);
      if (regnum == -1)
	return;
      iword |= ((regnum & 0xf) << 8);
    }

    p = frag_more(4);  
    md_number_to_chars(p, iword, 4);
    break;
  case BEXKAT1_IMM:
    iword = (BEXKAT1_IMM << 30) | (opcode->opcode << 26);
    p = frag_more(4);

    if (opcode->args == 2) {
      regnum = parse_regnum(&op_end);
      if (regnum == -1)
	return; 
      while (ISSPACE(*op_end))
	op_end++;
      if (*op_end != ',') {
	as_bad(_("missing comma: %s"), op_end);
	return;
      }
      op_end++;
      while (ISSPACE(*op_end))
	op_end++;
      iword |= (regnum & 0xf) << 16;
      md_number_to_chars(p, iword, 4);
      op_end = parse_exp_save_ilp(op_end, &arg);
      if (target_big_endian)
	fix_new_exp(frag_now,
		    (p - frag_now->fr_literal),
		    4,
		    &arg,
		    0,
		    BFD_RELOC_16);
      else
	fix_new_exp(frag_now,
		    (p - frag_now->fr_literal),
		    4,
		    &arg,
		    0,
		    BFD_RELOC_16);
    } else {
      op_end = parse_exp_save_ilp(op_end, &arg);
      md_number_to_chars(p, iword, 4);
      if (target_big_endian)
	fix_new_exp(frag_now,
		    (p - frag_now->fr_literal),
		    4,
		    &arg,
		    TRUE,
		    BFD_RELOC_16_PCREL);
      else
	fix_new_exp(frag_now,
		    (p - frag_now->fr_literal),
		    4,
		    &arg,
		    TRUE,
		    BFD_RELOC_16_PCREL);
    }
    break;
  case BEXKAT1_REGIND:
    iword = (BEXKAT1_REGIND << 30) | (opcode->opcode << 26);

    if (opcode->args == 3) {
      regnum = parse_regnum(&op_end);
      if (regnum == -1)
	return; 
      while (ISSPACE(*op_end))
	op_end++;
      if (*op_end != ',') {
	as_bad(_("missing comma: %s"), op_end);
	return;
      }
      op_end++;
      iword |= (regnum & 0xf) << 16;
    }
    while (ISSPACE(*op_end))
      op_end++;
    if (*op_end != '(')
      op_end = parse_exp_save_ilp(op_end, &arg);
    else { // Implicit 0 offset to allow for indirect
      arg.X_op = O_constant;
      arg.X_add_number = 0;
    }
      
    if (*op_end != '(') {
      as_bad(_("missing open paren: %s"), op_end);
      ignore_rest_of_line();
      return;
    }
    op_end++; // burn paren
    while (ISSPACE(*op_end))
      op_end++;
    regnum = parse_regnum(&op_end);
    if (regnum == -1)
      return;
    if (*op_end != ')') {
      as_bad(_("missing close paren: %s"), op_end);
      ignore_rest_of_line();
      return;
    }
    op_end++;
    iword |= (regnum & 0xf) << 12;
    p = frag_more(4);
    if (arg.X_op != O_constant) {
      as_bad(_("offset is not a constant expression"));
      ignore_rest_of_line();
      return;
    }
    offset = arg.X_add_number;
    if (offset < -32768 || offset > 32767) {
      as_bad(_("offset is out of range: %d\n"), offset);
      ignore_rest_of_line();
      return;
    }
    iword |= ((offset & 0xf000) << 8) | (offset & 0xfff);
    md_number_to_chars(p, iword, 4);
    break;
  case BEXKAT1_DIR:
    iword = (BEXKAT1_DIR << 30) | (opcode->opcode << 24);
    if (opcode->args > 1) {
      regnum = parse_regnum(&op_end);
      if (regnum == -1)
	return; 
      while (ISSPACE(*op_end))
	op_end++;
      if (*op_end != ',') {
	as_bad(_("missing comma: %s"), op_end);
	return;
      }
      op_end++;
      iword |= (regnum & 0xf) << 16;
    }
    if (opcode->args > 2) {
      regnum = parse_regnum(&op_end);
      if (regnum == -1)
	return; 
      while (ISSPACE(*op_end))
	op_end++;
      if (*op_end != ',') {
	as_bad(_("missing comma: %s"), op_end);
	return;
      }
      op_end++;
      iword |= (regnum & 0xf) << 12;
    }
    // We have a few opcodes (30, 31, 1x) that have an additional word with an address
    // the other opcodes use a value similar to REGIND, and trap doesn't use any registers
    // we figured out the registers above, so now it's just the addressing
    op_end = parse_exp_save_ilp(op_end, &arg);
    p = frag_more(4);
    if (opcode->opcode == 0x30 || opcode->opcode == 0x31 || opcode->opcode & 0x10) {
      md_number_to_chars(p, iword, 4);
      p = frag_more(4);
      fix_new_exp(frag_now,
		(p - frag_now->fr_literal),
		4,
		&arg,
		0,
		BFD_RELOC_32);
    } else {
      if (arg.X_op != O_constant) {
        as_bad(_("offset is not a constant expression"));
        ignore_rest_of_line();
        return;
      }
      offset = arg.X_add_number;
      if (offset < -32768 || offset > 32767) {
        as_bad(_("offset is out of range: %d\n"), offset);
        ignore_rest_of_line();
        return;
      }
      iword |= ((offset & 0xf000) << 8) | (offset & 0xfff);
      md_number_to_chars(p, iword, 4);
    }
    break;
  }

  while (ISSPACE(*op_end))
    op_end++;
  if (*op_end != 0)
    as_warn("extra stuff on line ignored %s %c", op_start, *op_end);
  if (pending_reloc)
    as_bad("Something forgot to clean up\n");
  return;
}

/* Turn a string in input_line_pointer into a floating point constant of type
   type. */

char *
md_atof(int type, char *litP, int *sizeP)
{
  int prec;
  LITTLENUM_TYPE words[4];
  char *t;
  int i;

  switch (type) {
  case 'f':
    prec = 2;
    break;
  case 'd':
    prec = 4;
  default:
    *sizeP = 0;
    return _("bad call to md_atof");
  }

  t = atof_ieee(input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * 2;

  for (i = prec-1; i >= 0; i--) {
    md_number_to_chars(litP, (valueT) words[i], 2);
    litP += 2;
  }
  return NULL;
}

const char *md_shortopts = "";

enum options {
  OPTION_EB = OPTION_MD_BASE,
  OPTION_EL
};

struct option md_longopts[] =
{
  { "EB", no_argument, NULL, OPTION_EB},
  { "EL", no_argument, NULL, OPTION_EL},
  {  NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof(md_longopts);

int
md_parse_option(int c, char *arg ATTRIBUTE_UNUSED)
{
  target_big_endian = 1;
  switch (c) {
  case OPTION_EB:
    target_big_endian = 1;
    break;
  case OPTION_EL:
    target_big_endian = 0;
    break;
  default:
    return 0;
  }
  return 1;
}

void
md_show_usage(FILE *stream)
{
  fprintf(stream, _("\
    -EB   big endian (default)\n\
    -EL   little endian\n"));
}

void
md_apply_fix(fixS *fixP, valueT *valP, segT seg ATTRIBUTE_UNUSED)
{
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  long val = *valP;
  valueT newval;

  switch (fixP->fx_r_type) {
  case BFD_RELOC_BEXKAT_15:
    if (val < -16383 || val > 16382)
      as_bad_where(fixP->fx_file, fixP->fx_line,
		   _("Constant out of 15 bit range for BFD_RELOC_BEXKAT_15"));
    val = ((val << 14) & 0x1e000000) | (val & 0x7ff);
    md_number_to_chars(buf, (int)val, 4);
    break;
  case BFD_RELOC_16:
    if (!val)
      break;
    newval = md_chars_to_number(buf, 4);
    newval |= ((val & 0xf000) << 8) | (val & 0xfff);
    md_number_to_chars(buf, (int)newval, 4);
    break;
  case BFD_RELOC_32:
    md_number_to_chars(buf, (int)val, 4);
    break;
  case BFD_RELOC_16_PCREL:
    if (!val)
      break;
    if (val < -32768 || val > 32767)
      as_bad_where(fixP->fx_file, fixP->fx_line,
		   _("pcrel too far BFD_RELOC_16_PCREL (%ld)"), val);
    newval = md_chars_to_number(buf, 4);
    newval |= ((val & 0xf000) << 8) | (val & 0xfff);
    /*    printf("applying fix reloc16_pcrel val =  %08lx, where = %lu, newval = %08lx\n",
	  val, fixP->fx_where, newval); */
    md_number_to_chars(buf, (int)newval, 4);
    break;
  default:
    as_fatal (_("Line %d: unknown relocation type: 0x%x."),
	      fixP->fx_line, fixP->fx_r_type);
  }

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

void
md_number_to_chars(char *ptr, valueT use, int nbytes)
{
  if (target_big_endian) {
    number_to_chars_bigendian(ptr, use, nbytes);
  } else {
    number_to_chars_littleendian(ptr, use, nbytes);
  }
}

static valueT
md_chars_to_number (char * buf, int n)
{
  valueT result = 0;
  unsigned char * where = (unsigned char *) buf;

  if (target_big_endian)
    {
      while (n--)
	{
	  result <<= 8;
	  result |= (*where++ & 255);
	}
    }
  else
    {
      while (n--)
	{
	  result <<= 8;
	  result |= (where[n] & 255);
	}
    }

  return result;
}

long
md_pcrel_from(fixS *fixP)
{
  valueT addr = fixP->fx_where + fixP->fx_frag->fr_address;

  switch (fixP->fx_r_type) {
  case BFD_RELOC_16_PCREL:
    if (target_big_endian)
      return addr + 4;
    else
      return addr;
  default:
    abort();
  }
  return addr;
}

arelent *
tc_gen_reloc(asection *section ATTRIBUTE_UNUSED, fixS *fixp)
{
  arelent *rel;
  bfd_reloc_code_real_type r_type;

  rel = xmalloc(sizeof(arelent));
  rel->sym_ptr_ptr = xmalloc(sizeof(asymbol *));
  *rel->sym_ptr_ptr = symbol_get_bfdsym(fixp->fx_addsy);
  rel->address = fixp->fx_frag->fr_address + fixp->fx_where;

  r_type = fixp->fx_r_type;

  rel->addend = fixp->fx_offset; 
  rel->howto = bfd_reloc_type_lookup(stdoutput, r_type);

  if (rel->howto == NULL) {
    as_bad_where(fixp->fx_file, fixp->fx_line,
                 _("Cannot represent relocation type %s"),
                 bfd_get_reloc_code_name(r_type));
    rel->howto = bfd_reloc_type_lookup(stdoutput, BFD_RELOC_32);
    gas_assert(rel->howto != NULL);
  }

  /* Since we use Rel instead of Rela, encode the vtable entry to be
     used in the relocation's section offset.  */
  if (fixp->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    rel->address = fixp->fx_offset;

  return rel;
}
