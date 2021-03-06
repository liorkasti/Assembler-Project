#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "utils.h"
#include "data.h"
#include "parse.h"
#include "hashtable.h"

/**************************************************************************
The size of the all memory is 2000 and it is not necessarly divided equally.
It was more useful to use dynamic data Structure! (-3)   
****************************************************************************/
/* array to store the data parsed from the code */
data_line data_arr[MAX_ARR_SIZE];
/* linked list for the symbols addresses of data pointers */
static int_hash_node *data_symtab[HASHSIZE];
/* linked list for the symbols addresses of instructions pointers */
static int_hash_node *inst_symtab[HASHSIZE];
/* linked list for the external commands */
static int_hash_node *exttab[HASHSIZE];
/* linked list for the opcode replacements */
static int_hash_node *optab[HASHSIZE];
/* temporary node for lookuping in the linkde lists */
int_hash_node *ihn;
/* array for entries */
char *entry_arr[MAX_ARR_SIZE];

/****************************************************
It is recommended to avoid using global variables. (-3)
*****************************************************/


/* counters for entries end externals */


int entry_counter, extern_counter;
/* general counter, and data and instructions counter */
int i, ic, dc;
/* flags for decide if to create the files */
char error_flag = 0, extern_flag = 0, entry_flag = 0;

/* this function is checking if there is a symbol. if yes, its returning a pointer to it, if not, returns null */
char *get_symbol(code_line *c_line)
{
    char *tmp = (c_line->line);
	/* while its not end of line */
    while (*(c_line->line) != '\0')
    {
		/* if there is a : char, its probably has symbol */
        if (*(c_line->line) == ':')
        {
			/* put null char at end of symbol */
            *((c_line->line)++) = '\0';
			/* make sure its starts with a char */
            if (!isalpha(tmp[0]))
            {
                ERROR("Symbol should start with a char", c_line->line_number)
                error_flag = 1;
            }
            return tmp;
        }
        (c_line->line)++;
    }
    c_line->line = tmp;
    return NULL;
}

/* this function extract the data from a data command */
void extract_data(code_line *c_line)
{
    int sign = 1;
    int sum;
	char expecting_number = 0;
	/* while its not end of line */
    while ((*c_line).line[0] != '\n' && (*c_line).line[0] != '\0')
    {
		/* remove the spaces from the beginning of the line */
		remove_pre_spaces(&((*c_line).line));
		/* check sign */

		sign = 1;

		if (c_line->line[0] == '-')
        {
            sign = -1;
            (c_line->line)++;
        }

        else if (c_line->line[0] == '+')
        {
            sign = 1;
            (c_line->line)++;
        }

        /* make sure its start with digit */
        if (!isdigit((*c_line).line[0]))
        {
            ERROR("Wrong .data format, expecting number", (*c_line).line_number)
			error_flag = 1;
        }
        else
        {
            sum = 0;
            while (isdigit((*c_line).line[0]))
            {
                sum *= 10;
                sum += ((*c_line).line[0]) - '0';
                (c_line->line)++;
            }
			/* add the number to data arr */
            data_arr[dc++] = num2data(sum * sign);
        }
		
		remove_pre_spaces(&((*c_line).line));
		/* if there is more numbers */
        if (c_line->line[0] == ',')
		{
			expecting_number = 1;
			(c_line->line)++;
		}
		else
		{
			expecting_number = 0;
		}
    }
	/* if the line ended and we still waiting for another number */
	if (expecting_number == 1)
	{
		ERROR("Expecting number after \',\'", (*c_line).line_number)
		error_flag = 1;
	}

}

/* this function extract the string from a string command */
void extract_string(code_line *c_line)
{
    remove_pre_spaces(&(c_line->line));
	/* make sure its starts with " */
    if (!(c_line->line[0] == '\"'))
    {
        ERROR(".string argument must start with \"", (*c_line).line_number)
		error_flag = 1;
        return;
    }

    (c_line->line)++;
	/* while its not reached the second " */
    while ((c_line->line[0] != '\"') && (c_line->line[0] != '\0'))
    {
		/* add the new string to the array */
        data_arr[dc++] = char2data(c_line->line[0]);
        (c_line->line)++;
    }

	/* at end, make sure it ends with " */
    if (c_line->line[0] == '\"')
    {
        data_arr[dc++] = char2data('\0');
    }
    else
    {
        ERROR(".string argument must end with \"", (*c_line).line_number)
		error_flag = 1;
        return;
    }
    
}

/* this function extract the entry from a entry command */
void extract_entry(code_line *c_line)
{
	/* skip on tabs and spaces */
	c_line->line = strtok(c_line->line, " \t\n");
	/* if there is no entry */
	if (!c_line->line)
	{
        ERROR("No value found after entry command", (*c_line).line_number)
		error_flag = 1;
		return;
	}
	/* add entry to entries array */
	entry_arr[entry_counter++] = strdup(c_line->line);
	c_line->line = strtok(NULL, " \t\n");
	/* make sure there is no other words */
	if (c_line->line)
	{
        ERROR("Unexpected words in entry command", (*c_line).line_number)
		error_flag = 1;
	}
}

/* this function extract the entry from a entry command */
void extract_extern(code_line *c_line)
{
	/* skip on tabs and spaces */
	c_line->line = strtok(c_line->line, " \t\n");
	/* if there is no extern */
	if (!c_line->line)
	{
        ERROR("No value found after extern command", (*c_line).line_number)
		error_flag = 1;
		return;
	}
	/* add to the extern hashtable with value 0 */
    int_install(c_line->line, '0', exttab);
	extern_counter++;
	c_line->line = strtok(NULL, " \t\n");
	/* make sure there is no other words */
	if (c_line->line)
	{
        ERROR("Unexpected words in extern command", (*c_line).line_number)
		error_flag = 1;
	}
}

/* this functions returnes the number of operands expected per opcode */
char count_exp_operands(char *s)
{
	if (strcmp(s, "rts") == 0 || strcmp(s, "stop") == 0)
	{
		return 0;
	}
	else if (strcmp(s, "mov") == 0 || strcmp(s, "cmp") == 0 || strcmp(s, "add") == 0 ||
			strcmp(s, "sub") == 0 || strcmp(s, "lea") == 0)
	{
		return 2;
	}
	else
	{
		return 1;
	}
}

/* this function parsing the instruction given in the line */
void parse_instruction(code_line *c_line)
{
	/* count how many instruction lines this line using, including repeats */
    int local_ic = 0;
	char num_of_exp_operands;
	char *tmp;

	/* duplicate the line in tmp to extract the repeat parameter */
	tmp = strdup(c_line->line);

	/* isolate the opcode */
	c_line->line = strtok(c_line->line, "12");

	/* find it in the opcode hashtable */
	ihn = int_lookup(c_line->line, optab);
	/* if its not a valid opcode */
	if (!ihn)
	{
		/* check if its a comment or empty line */
        if ((c_line->line[0] == ';') || (c_line->line[0] = '\n'))
        {
            c_line->done = 1;
            return;
        }
        ERROR("Ilegal instruction", (*c_line).line_number)
		error_flag = 1;
		return;
	}

	/* make sure the repeat value is legal */
	if ((tmp[strlen(c_line->line)] < '1') || (tmp[strlen(c_line->line)] > '2'))
	{
		ERROR("Wrong value for repeat. must be between 1 and 3", (*c_line).line_number)
		error_flag = 1;
		return;
	}

	c_line->repeat = tmp[strlen(c_line->line)] - '0';


	/* use the auxiliary function to count the expected operands */
	num_of_exp_operands = count_exp_operands(c_line->line);
	c_line->instruction = (instruction_line *) malloc(sizeof(instruction_line) * 8);

	/* clear the instruction line */
    init_instruction_line(c_line->instruction);

    /* set the group number */
    c_line->instruction->group = num_of_exp_operands;

    /* set the opcode */
    c_line->instruction->opcode = ihn->defn;

    /* increment the ic for instruction */
    local_ic++;

    /* seek for null char */
	while (c_line->line[0] != '\0')
    {
        (c_line->line)++;
    }
    (c_line->line)++;

	remove_pre_spaces(&(c_line->line));

	/* read the expected number of operands */
    c_line->line = strtok(c_line->line, " \t,\n");
	while (num_of_exp_operands-- > 0)
	{
        if (num_of_exp_operands == 1)
        {
			/* its the first operand */
            c_line->src_opr = strdup(c_line->line);
        }
        else
        {
			/* its the second operand */
            c_line->dest_opr = strdup(c_line->line);
        }
		/* check for the type of the oprands and decide how much ic should be incremented */
        switch (c_line->line[0])
		{
			case '#': /* if number: */
                local_ic++;
                if (num_of_exp_operands == 1)
                {
                    c_line->instruction->src_addr = 0;
                }
                else
                {
                    c_line->instruction->dest_addr = 0;
                }
                break;
			case 'r': /* if register: */
                (c_line->line)++;
                if ((c_line->line[0] >= '0') && (c_line->line[0] <='7'))
                {
                    if (num_of_exp_operands == 1)
                    {
                    	local_ic++;
                    	c_line->src_reg = c_line->line[0] - '0';
                        free(c_line->src_opr);
                        c_line->src_opr = NULL;
                        c_line->instruction->src_addr = 3;
                    }
                    else
                    {
                        if( c_line->instruction->src_addr != 3) local_ic++;
                    	c_line->dest_reg = c_line->line[0] - '0';
                        free(c_line->dest_opr);
                        c_line->dest_opr = NULL;
                        c_line->instruction->dest_addr = 3;
                    }
                }
                break;
		    default:
                if (strstr(c_line->line, "$$")) /* if previous copy: */
                {
                     if (num_of_exp_operands == 1)
                    {
                    	if((c_line-1)->src_opr != NULL || (c_line-1)->dest_opr != NULL || (c_line-1)->dest_reg != REG_EMPTY || (c_line-1)->src_reg != REG_EMPTY)
                    	{
                       		local_ic++;
                    		if((c_line-1)->instruction->group == 2)
                    		{
                    		/* copy the first operand depending on the number of previous operands: */
                    			if(c_line->instruction->group == 2)
                    			{
                    				/* copy previous data to current data: */
                    				c_line->src_reg = (c_line-1)->src_reg;
                        			c_line->instruction->src_addr = (c_line-1)->instruction->src_addr;
                        			if((c_line-1)->src_opr) c_line->src_opr = strdup((c_line-1)->src_opr);
                    			}
                    			else if(c_line->instruction->group == 1)
                    			{
                    				/* copy previous data to current data: */
                                	c_line->dest_reg = (c_line-1)->src_reg;
                        			c_line->instruction->dest_addr = (c_line-1)->instruction->src_addr;
                        			if((c_line-1)->src_opr) c_line->dest_opr = strdup((c_line-1)->src_opr);
                    			}
                    			else
                    			{
                            		ERROR("Wrong value for $$. the internal_1 group is not valid", (*c_line).line_number)
                            		error_flag = 1;
                            		return;
                    			}
                    		}
                    		else if((c_line-1)->instruction->group == 1)
                    		{
                    			if(c_line->instruction->group == 2)
                    			{
                    				/* copy previous data to current data: */
                                	c_line->src_reg = (c_line-1)->dest_reg;
                        			c_line->instruction->src_addr = (c_line-1)->instruction->dest_addr;
                        			if((c_line-1)->dest_opr) c_line->src_opr = strdup((c_line-1)->dest_opr);
                    			}
                    			else if(c_line->instruction->group == 1)
                    			{
                    			/* copy previous data to current data: */
                                	c_line->dest_reg = (c_line-1)->dest_reg;
                        			c_line->instruction->dest_addr = (c_line-1)->instruction->dest_addr;
                        			if((c_line-1)->dest_opr) c_line->dest_opr = strdup((c_line-1)->dest_opr);
                    			}
                    			else
                    			{
                            		ERROR("Wrong value for $$. the internal_2 group is not valid", (*c_line).line_number)
                            		error_flag = 1;
                            		return;
                    			}
                    		}
                    		else
                    		{
                        		ERROR("Wrong value for $$. the external group is not valid", (*c_line).line_number)
                        		error_flag = 1;
                        		return;
                    		}
                    	}
                    	else
                    	{
                    		ERROR("Wrong value for $$. $$ operand must be before line instruction", (*c_line).line_number)
                    		error_flag = 1;
                    		return;
                    	}
                    }
                    else
                    {
					/* copy the second operand depending on the number of previous operands: */
                    	if((c_line-1)->src_opr != NULL || (c_line-1)->dest_opr != NULL || (c_line-1)->dest_reg != REG_EMPTY || (c_line-1)->src_reg != REG_EMPTY)
                    	{
                       		local_ic++;
                    		if((c_line-1)->instruction->group == 2)
                    		{
                    			if(c_line->instruction->group == 2)
                    			{
                    			/* copy previous data to current data: */
                                	c_line->dest_reg = (c_line-1)->dest_reg;
                        			c_line->instruction->dest_addr = (c_line-1)->instruction->dest_addr;
                        			if((c_line-1)->dest_opr) c_line->dest_opr = strdup((c_line-1)->dest_opr);
                    			}
                    			else if(c_line->instruction->group == 1)
                    			{
                    			/* copy previous data to current data: */
                                	c_line->dest_reg = (c_line-1)->dest_reg;
                        			c_line->instruction->dest_addr = (c_line-1)->instruction->dest_addr;
                        			if((c_line-1)->dest_opr) c_line->dest_opr = strdup((c_line-1)->dest_opr);
                    			}
                    			else
                    			{
                            		ERROR("Wrong value for $$. the internal_1 group is not valid", (*c_line).line_number)
                            		error_flag = 1;
                            		return;
                    			}
                    		}
                    		else if((c_line-1)->instruction->group == 1)
                    		{
                    			if(c_line->instruction->group == 2)
                    			{
                    			/* copy previous data to current data: */
                                	c_line->dest_reg = (c_line-1)->dest_reg;
                        			c_line->instruction->dest_addr = (c_line-1)->instruction->dest_addr;
                        			if((c_line-1)->dest_opr) c_line->dest_opr = strdup((c_line-1)->dest_opr);
                    			}
                    			else if(c_line->instruction->group == 1)
                    			{
                    			/* copy previous data to current data: */
                                	c_line->dest_reg = (c_line-1)->dest_reg;
                        			c_line->instruction->dest_addr = (c_line-1)->instruction->dest_addr;
                        			if((c_line-1)->dest_opr) c_line->dest_opr = strdup((c_line-1)->dest_opr);
                    			}
                    			else
                    			{
                            		ERROR("Wrong value for $$. the internal_2 group is not valid", (*c_line).line_number)
                            		error_flag = 1;
                            		return;
                    			}
                    		}
                    		else
                    		{
                        		ERROR("Wrong value for $$. the external group is not valid", (*c_line).line_number)
                        		error_flag = 1;
                        		return;
                    		}
                    	}
                    	else
                    	{
                    		ERROR("Wrong value for $$. $$ operand must be before line instruction", (*c_line).line_number)
                    		error_flag = 1;
                    		return;
                    	}
                  }
                }
			else
			{
				local_ic++;
				if (num_of_exp_operands == 1)
				{
					c_line->instruction->src_addr = 1;
				}
				else
				{
					c_line->instruction->dest_addr = 1;
				}
			}
		}
        c_line->line = strtok(NULL, " \t,\n");
	}
    ic += (local_ic * c_line->repeat);

}


/* this function parsing the commands line (string, data, extern and entry) */
void parse_command(code_line *c_line, char *symbol)
{
	/* if string */
	if (strncmp(c_line->line, ".string", sizeof(".string") - 1) == 0)
	{
		/* if there is a symbol */
		if (symbol)
		{
			ihn = int_install(symbol, dc, data_symtab);
		}
        c_line->line += (sizeof(".string") - 1);
		/* use the auxiliary function to extract the string */
		extract_string(c_line);
	}
	/* if data */
	else if (strncmp((*c_line).line, ".data", sizeof(".data") - 1) == 0)
	{
		if (symbol)
		{
			ihn = int_install(symbol, dc, data_symtab);
		}
        c_line->line += (sizeof(".data") - 1);
		/* use the auxiliary function to extract the data */
		extract_data(c_line);
	}
	/* if entry */
	else if (strncmp(c_line->line, ".entry", sizeof(".entry") - 1) == 0)
	{
        c_line->line += (sizeof(".entry") - 1);
		/* use the auxiliary function */
		extract_entry(c_line);
	}
	/* if extern */
	else if (strncmp(c_line->line, ".extern", sizeof(".extern") - 1) == 0)
	{
        c_line->line += (sizeof(".extern") - 1);
		/* use the auxiliary function */
		extract_extern(c_line);
	}
	else
	{
		ERROR("Unrecognized command", (*c_line).line_number)
		error_flag = 1;
	}
    c_line->done = 1;
}

/* this function is doing the first parsing to the line, and leaving for the second parsing to check for the symbols addresses and parsing the operands */
int first_parsing(code_line *file, int num_of_lines)
{
    char *symbol;
	error_flag = 0;
	ic = 0;
	dc = 0;
	extern_counter = 0;
	entry_counter = 0;

    for (i = 0; i < num_of_lines; i++) /* loop over all the code lines */
    {
        symbol = get_symbol(file + i);
        remove_pre_spaces(&(file[i].line));
		if (file[i].line[0] == '.')
		{
			parse_command(&file[i], symbol);
		}
		else
		{
            if (symbol)
            {
                ihn = int_install(symbol, ic, inst_symtab);
            }
			parse_instruction(&file[i]);
		}
    }

    return 0;
}

/* this function is for the second parsing, parsing each operand */
void parse_operand(char *op, int addr, FILE *obj, FILE *ext, int *line_count, int org_line_num)
{
	/* 2 temporary arrays to get the base 4 results */
	char base_result[MAX_DIGIT + 1], base_result1[MAX_DIGIT + 1];
	int sign = 1, sum = 0;
	data_line dl;
	/* check what type of operand */
	switch (addr)
	{
		case 0: /* direct addressing */
			op++;
			if (op[0] == '-')
			{
				sign = -1;
				op++;
			}
			while (isdigit(op[0]))
			{
				sum *= 10;
				sum += op[0] - '0';
				op++;
			}
			/* write the given number to the obj file */
			dl = num2data(sum * sign);
			dl.data<<=2;
			fprintf(obj, "%s\t%s\n",to_base((*line_count) + LINE_OFFSET, BASE, base_result, NO_PAD) , to_base(dl.data | ERA_A, BASE, base_result1, PAD));
            (*line_count)++;
			break;
		case 1: /* its symbol */
			/* check in the data hashtable */
			if ((ihn = int_lookup(op, data_symtab)))
			{
				/* write the symbol address to the obj file */
				dl = num2data(ihn->defn + ic + LINE_OFFSET + 1);
				dl.data<<=2;
				fprintf(obj, "%s\t%s\n", to_base((*line_count) + LINE_OFFSET, BASE, base_result, NO_PAD), to_base(dl.data | ERA_R, BASE, base_result1, PAD));
                (*line_count)++;
			}
			/* check in the instruction hashtable */
            else if ((ihn = int_lookup(op, inst_symtab)))
            {
				/* write the symbol address to the obj file */
				dl = num2data(ihn->defn + LINE_OFFSET + 1);
				dl.data<<=2;
            }
			/* check in the externals hashtable */
            else if ((ihn = int_lookup(op, exttab)))
			{
				/* write 0 to the obj file */
				fprintf(obj, "%s\t%s\n", to_base((*line_count)+ LINE_OFFSET, BASE, base_result, NO_PAD), to_base(num2data(0).data | ERA_E, BASE, base_result1, PAD));
				/* add line to the ext file */
				fprintf(ext, "%s\t%s\n", op, to_base(*line_count + LINE_OFFSET, BASE, base_result, NO_PAD));
				(*line_count)++;
			}
            else
            {
                ERROR("Can't find symbol in symbol table or in extern table", org_line_num)
                error_flag = 1;
            }
            break;
	}

}

/* this function checks if the given addressing is valid */
void check_addressing(instruction_line *inst_line, int line_number)
{
    switch(inst_line->opcode)
    {
        case MOV:
        case ADD:
        case SUB:
            if ((inst_line->dest_addr == 0) || (inst_line->dest_addr == 2))
            {
                ERROR("Ilegal addressing",line_number)
                error_flag = 1;
            }
            break;
        case NOT:
        case CLR:
        case INC:
        case DEC:
            if ((inst_line->dest_addr == 0) || (inst_line->dest_addr == 2) || (inst_line->src_addr != 0))
            {
                ERROR("Ilegal addressing",line_number)
                error_flag = 1;
            }
            break;
        case JMP:
        case BNE:
        case RED:
            if ((inst_line->dest_addr == 0) || (inst_line->src_addr != 0))
            {
                ERROR("Ilegal addressing",line_number)
                error_flag = 1;
            }
            break;
        case RTS:
        case STOP:
            if ((inst_line->dest_addr != 0) || (inst_line->src_addr != 0))
            {
                ERROR("Ilegal addressing",line_number)
                error_flag = 1;
            }
            break;
        case LEA:
            if ((inst_line->dest_addr == 0) || (inst_line->dest_addr == 2) || (inst_line->src_addr != 1))
            {
                ERROR("Ilegal addressing",line_number)
                error_flag = 1;
            }
            break;
        case PRN:
            if (inst_line->src_addr != 0)
            {
                ERROR("Ilegal addressing",line_number)
                error_flag = 1;
            }
            break;
        case JSR:
            if ((inst_line->dest_addr != 1) || (inst_line->src_addr != 0))
            {
                ERROR("Ilegal addressing",line_number)
                error_flag = 1;
            }
            break;

    }
}

/* this function is doing the second parsing for the line, resolving the symbols and parsing the operands */
int second_parsing(code_line *file, int num_of_lines, char *module_name)
{
	/* temporary arrays for converting to base 4 */
	char base_result[MAX_DIGIT + 1], base_result1[MAX_DIGIT +1], file_name[MAX_FILENAME];
    int i, j, line_count = 0;
    data_line dl, dl_src_reg, dl_dest_reg;
	/* pointers for the files */
    FILE *obj, *ext, *ent;
	/* open all the relevant files */
    sprintf(file_name, "%s.obj", module_name);
    obj = fopen(file_name, "w");
    sprintf(file_name, "%s.ext", module_name);
    ext = fopen(file_name, "w");
    sprintf(file_name, "%s.ent", module_name);
    ent = fopen(file_name, "w");
	/* print header */
    fprintf(obj, "%s %s\n", to_base(ic, BASE, base_result, NO_PAD), to_base(dc, BASE, base_result1, NO_PAD));
    line_count++;
    for (i = 0; i < num_of_lines; i++) /* loop over all the code lines */
    {
		/* only if this file need more parsing */
        if (!file[i].done)
        {
			/* check if the addressing is valid */
            check_addressing(file[i].instruction, file[i].line_number);
			/* parse the operands * repeat times */
            for (j = 0; j < file[i].repeat; j++)
            {
				/* write to file the actual instruction */
                fprintf(obj, "%s\t%s\n",to_base(line_count + LINE_OFFSET, BASE, base_result, NO_PAD) , to_base(bline2data(*(file[i].instruction)).data | ERA_A, BASE, base_result1, PAD));
                line_count++;

				/* if there is register addressing */
                if ((*file[i].instruction).src_addr == 3 || (*file[i].instruction).dest_addr == 3)
                {
					/* clear register data */
					if (file[i].src_reg == REG_EMPTY) file[i].src_reg = 0;
					/* clear register data */
					if (file[i].dest_reg == REG_EMPTY) file[i].dest_reg = 0;
					/* clear dl data */
					dl.data = 0;
					dl_src_reg.data = file[i].src_reg;
					dl_dest_reg.data = file[i].dest_reg;
					if ((*file[i].instruction).group == 2)
					{
						dl.data = ((dl_src_reg.data <<= 7) | (dl_dest_reg.data <<= 2));
					}
					else if ((*file[i].instruction).group == 1)
					{
						dl.data = ((dl_dest_reg.data <<= 7) | (dl_src_reg.data <<= 2));
					}
					else
					{
						fprintf(stderr, "Error, can't find assress for register %s\n", entry_arr[i]);
						error_flag = 1;
					}
					/* write to file the actual register data information */
					fprintf(obj, "%s\t%s\n",to_base(line_count + LINE_OFFSET, BASE, base_result, NO_PAD) , to_base(dl.data | ERA_A, BASE, base_result1, PAD));
					line_count++;
                }

				/* if there is source operand */
                if (file[i].src_opr)
                {
					/* use the auxiliary function to parse the operand */
					parse_operand(file[i].src_opr, file[i].instruction->src_addr, obj, ext, &line_count, file[i].line_number);
                }
				/* if there is source operand */
                if (file[i].dest_opr)
                {
					/* use the auxiliary function to parse the operand */
					parse_operand(file[i].dest_opr, file[i].instruction->dest_addr, obj, ext, &line_count, file[i].line_number);
                }
            }
        }
    }
    /* write to obj file the data array */
    for (i = 0; i < dc; i++)
    {
        fprintf(obj, "%s\t%s\n", to_base(line_count + LINE_OFFSET, BASE, base_result, NO_PAD), to_base(data_arr[i].data, BASE, base_result1, PAD));
        line_count++;
    }
	/* write to ent file the entries */
    for (i = 0; entry_arr[i]; i++)
    {
		/* try to find the address in the data hashtable */
        if ((ihn = int_lookup(entry_arr[i], data_symtab)))
        {
            fprintf(ent, "%s\t%s\n", entry_arr[i], to_base(ihn->defn + LINE_OFFSET + 1 + ic, BASE, base_result, NO_PAD));
        }
		/* try to find the address in the data hashtable */
        else if((ihn = int_lookup(entry_arr[i], inst_symtab)))
        {
            fprintf(ent, "%s\t%s\n", entry_arr[i], to_base(ihn->defn + LINE_OFFSET + 1, BASE, base_result, NO_PAD));
        }
        else
        {
            fprintf(stderr, "Error, can't find assress for entry %s\n", entry_arr[i]);
            error_flag = 1;
        }
    }
	/* close all the open files */
    fclose(obj);
    fclose(ext);
    fclose(ent);
	
	/* clear the hashtable for the next file */
	for (i = 0; i < HASHSIZE; i++)
	{
		data_symtab[i] = NULL;
		inst_symtab[i] = NULL;
		exttab[i] = NULL;
	}
	/* if there is an error, delete all the files */
    if (error_flag)
    {
        sprintf(file_name, "%s.obj", module_name);
        remove(file_name);
        sprintf(file_name, "%s.ext", module_name);
        remove(file_name);
        sprintf(file_name, "%s.ent", module_name);
        remove(file_name);
    }
	/* if there was'nt any externs */
    else if (extern_counter == 0)
    {
        sprintf(file_name, "%s.ext", module_name);
        remove(file_name);
    }
	/* if there was'nt any externs */
    else if (entry_counter == 0)
    {
        sprintf(file_name, "%s.ent", module_name);
        remove(file_name);
    }
    return 0;
}

/* this function is called only in start of day, and loading all the opcodes into the hashtable */
void init_op_table()
{
    ihn = int_install("mov" , MOV, optab);
    ihn = int_install("cmp" , CMP, optab);
    ihn = int_install("add" , ADD, optab);
    ihn = int_install("sub" , SUB, optab);
    ihn = int_install("not" , NOT, optab);
    ihn = int_install("clr" , CLR, optab);
    ihn = int_install("lea" , LEA, optab);
    ihn = int_install("inc" , INC, optab);
    ihn = int_install("dec" , DEC, optab);
    ihn = int_install("jmp" , JMP, optab);
    ihn = int_install("bne" , BNE, optab);
    ihn = int_install("red" , RED, optab);
    ihn = int_install("prn" , PRN, optab);
    ihn = int_install("jsr" , JSR, optab);
    ihn = int_install("rts" , RTS, optab);
    ihn = int_install("stop" , STOP, optab);
}
