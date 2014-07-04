/*
	Jordi Burbano UID: 204-076-325
	Brandon Wu		UID: 603-859-458
*/
// UCLA CS 111 Lab 1 command reading
#include "command.h"
#include "command-internals.h"
#include <error.h>
/* TODO: 	
		1. 	Proper error msg format
		2.	Better documentation
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "alloc.h"
#define BUFFER_SIZE 1024
#define WORD_COUNT 	10
#define WORD_SIZE		100
typedef enum
{
	TRUE,
	FALSE,
} boolean;

typedef enum
{
	WORD,				// A word not including whitespace or other token
	OPAREN,			// Left paren  (
	CPAREN,			// Right paren )
	AND,				// &&
	OR,					// ||
	PIPE,				// |
	SEMI,				// ;
	IN,					// <
	OUT,				// >
	COMMENT,		// #
	TICK,				// `
	INVALID,		// Invalid token (e.g. <<<)
} token_type;

typedef struct
{
	token_type type;
	char* word;
} token;

struct command_node
{
	command_t 						cmd;
	struct command_node* 	nxt;
};

struct command_stream
{
	/* command_stream implemented by linked list of commands*/
	struct command_node* list;
};

/* Determine if character is white space */
inline
boolean 
is_space(char c)
{
	switch(c)
	{
		case ' ':
		case '\t':
		case '\n':
		case '\v':
		case	'\f': return TRUE;
		default: return FALSE;
	}
};
/* Determine if special character */
inline
boolean
is_special(char c)
{
	switch(c)
	{
		case ';':
		case '|':
		case '&':
		case '<':
		case '>':
		case '(':
		case ')':
		case '#':
		case '`':
			return TRUE;
		default: return FALSE;
	}
};

inline
token_type
get_token_type(char* line_buffer, int pos, int len)
{
	char c = line_buffer[pos];	
	if (c=='(')
		return OPAREN;
	if (c==')')
		return CPAREN;
  if (c==';')
		return SEMI;
	if (c=='<')
		return IN;
	if (c=='>')
		return OUT;
	if (c=='#')
	{
		return COMMENT;
	}
	if (c=='&')
	{
		if(pos<len-1 && line_buffer[pos+1] == '&')
			return AND;
		else return INVALID;		// Invalid if single &
	}
	if (c == '|')
	{
		if (pos<len-1 && line_buffer[pos+1] == '|')
			return OR;
		else return PIPE;
	}
	else if (c == '`')
		return TICK;
	return INVALID;
}

/* 	GET_TOKEN
		Given line buffer and current pos within buffer, 
		get the next token in that line
		Return position of next char in buffer after end
		of token t. If no token exists, return INVALID */
int
get_token(char* line_buffer, int len, int pos, token* t)
{
	char c;
	int word_len = 0;
	if (pos >= len)
	{
		t->type = INVALID;
		return pos;
	}
	while((pos<len) && is_space(c=line_buffer[pos])==TRUE)
		++pos;

	if ((pos<len) && is_special(c) == FALSE)	// if it is a word token
	{
		t->type = WORD;
		t->word = checked_malloc(WORD_SIZE);

		//t->word[0] = c;
		while (pos<len && is_space(c=line_buffer[pos]) == FALSE  
						&& is_special(c) == FALSE && word_len < WORD_SIZE)
		{
			t->word[word_len] = c;
			word_len++;
			pos++;
		}
	}
	else	// If it is a valid special token
	{
		t->type = get_token_type(line_buffer, pos, len);
		if (t->type == COMMENT)
			return len;
		switch((t->type = get_token_type(line_buffer, pos, len)))
		{
			case AND:
			case OR:
				pos++;
			default: pos++;
		}
	}
	if (t->type == IN || t->type == OUT)
		if (line_buffer[pos-1] == line_buffer[pos])
	pos++;
	return pos;
}

/* 	Read a single line terminated by \n or EOF from file 
		RETURN len of line_buffer */
int
get_line(int (*get_next_byte) (void *), void* fp, char* line_buffer)
{
	char c;
	int len=0;
	c=(*get_next_byte)(fp);
	while((len<BUFFER_SIZE) && c != '\n' && (c>0)) 
	{
		line_buffer[len] = c;	
		len++;
		c=(*get_next_byte)(fp);
	}
	if (c < 0)		// If c is negative, return EOF
		return -1;
	if (len >= BUFFER_SIZE)
		error(1,0, "Buffer Overflow");
	return len;
}

/* Count num of words in line_buffer up to \n or special token */
int
count_words(char* line_buffer, int len)
{
	int i=0, num=0;
	char c;
	while (i < len)
	{
		while(i < len && is_space(c=line_buffer[i]) == TRUE
					&& is_special(c) == FALSE)
			i++;
		if (i < len && is_special(c) == FALSE)
			num++;
		while(i < len && is_space(c=line_buffer[i]) == FALSE
						&& is_special(c) == FALSE)
			i++;
		if(is_special(c) == TRUE)
		{
			break;
		}
	}
	return num;
}
int
get_io(char* line_buffer, int len, int pos, command_t cmd, int input)
{
	/* 	if input = 1, the redirect is input
			if input = 0, the redirect is an output	*/
	int i;
	char* word = checked_malloc(WORD_SIZE);
	token* t = checked_malloc(sizeof(token));
	if (input)
	{
		if (cmd->input != NULL)
			error(1,0, "Multiple I/O redirect");
		cmd->input = word;
	}
	else
	{
		if (cmd->output != NULL)
			error(1,0, "Multiple I/O Redirect");
		cmd->output = word;
	}
	i = get_token(line_buffer,len, pos, t);
	if (t->type != WORD)
		error(1,0, "I/O redirect needs to be followed by a word");
	memcpy(word, t->word, WORD_SIZE);
	free(t->word);
	get_token(line_buffer,len,i,t);
	if (t->type == WORD)
		error(1,0,"I/O redirect cannot be followed by > 1 word");
	return i;
}
int
make_simple_cmd(int (*get_next_byte) (void*), void* fp, char* line_buffer,
		int len, int pos, command_t cmd, int subshell)
{
	int num_words = count_words(line_buffer+pos, len-pos);
	int i = 0, wnum = 0;
	token* t = checked_malloc(sizeof(token));
	
	if (num_words == 0)
	{
		return -1;
	}
	cmd->type = SIMPLE_COMMAND;
	cmd->u.word = checked_malloc(sizeof(void*)*num_words+1);

	for (i=0; i<num_words; ++i)
		cmd->u.word[i] = checked_malloc(WORD_SIZE);

	i = get_token(line_buffer, len, pos, t);
	do
	{
		if (t->type==IN || t->type==OUT)
		{
			i = get_io(line_buffer, len, i, cmd, (t->type==IN));
		}
		else
			memcpy(cmd->u.word[wnum],t->word, WORD_SIZE);
		wnum++;
		i = get_token(line_buffer, len, i, t);
	}
	while ((wnum<num_words && t->type==WORD) || t->type==IN || t->type==OUT);
	
	token_type type = t->type;
	free(t->word);
	switch(type)
	{
		case AND:
		case OR:
			return i-2;
		case IN:
		case OUT:
		case INVALID:
			return i;
		default: return i-1;
	}
}	

command_t
make_command(int (*get_next_byte) (void*), void* fp, char* line_buffer,
		int len, int* line_num, int subshell)
{
	boolean done = FALSE;
	command_t cmd = NULL;
	token* t = checked_malloc(sizeof(token));
	int pos = 0;

	int temp;
	
	while(done == FALSE || subshell==1)
	{
		while(pos>=len)
		{
			len = get_line(get_next_byte, fp, line_buffer);
			if (len < 0)
			{
				if (subshell)
					error(1,0, "Subshell not terminated");
				else return NULL;	
			}
			
			pos = 0;
			*line_num += 1;
		}
		
		done = TRUE;
		
		while(pos < len)
		{
			temp = get_token(line_buffer, len, pos, t);	
			token_type type = t->type;
			if (type!=WORD)
				pos= temp;
			if (type==WORD)
			{
				cmd = checked_malloc(sizeof(struct command));
				pos = make_simple_cmd(get_next_byte, fp, line_buffer, len, pos,
						cmd, 0);
			}
			else if (type==IN || type==OUT)
			{
				error(1,0, "I/O not affiliated with simple command");
			} 
			else if (type==AND || type==OR || type==PIPE || type == SEMI)
			{
				if (cmd == NULL)
					error(1,0, "No left-hand operand");
	
				command_t tmp_cmd;
				tmp_cmd = checked_malloc(sizeof(struct command));
				tmp_cmd->u.command[1] = checked_malloc(sizeof(struct command));
				tmp_cmd->status = -1;
				
				if((type==PIPE || type==SEMI)&&(cmd->type==AND_COMMAND ||
					cmd->type == OR_COMMAND))
				{
					tmp_cmd->u.command[0] = cmd->u.command[1];
					cmd->u.command[1] = tmp_cmd;
				}
				else 
				{
					tmp_cmd->u.command[0] = cmd;
					cmd = tmp_cmd;
				}
				
				while((pos = make_simple_cmd(get_next_byte, fp, line_buffer, 
					len, pos,tmp_cmd->u.command[1], 0)) == -1)
				{
					len = get_line(get_next_byte, fp, line_buffer);		
					if (len == -1)
						error(1,0, "Syntax Error");
					pos = 0;
				}

				switch(type)
				{
					case AND:
						tmp_cmd->type = AND_COMMAND;
						break;
					case OR:
						tmp_cmd->type = OR_COMMAND;
						break;
					case PIPE:
						tmp_cmd->type = PIPE_COMMAND;
						break;
					case SEMI:
						tmp_cmd->type = SEQUENCE_COMMAND; 
						break;
					default:;
				}	
			}
			else if (type == OPAREN)
			{
				if (cmd != NULL)
					error(1,0,"Non empty cmd preceding '('");	
				cmd = checked_malloc(sizeof(struct command));
				cmd->status = -1;
				cmd->type = SUBSHELL_COMMAND;
				cmd->u.subshell_command = make_command(get_next_byte, fp,
					line_buffer+pos, len-pos, line_num, subshell+1);
				return cmd;
			}
			else if (type == CPAREN)
			{
				if (subshell== 1)
				{
					return cmd;
				}
				else if (subshell > 1)
					subshell--;
				else
					error(1,0,"Unmatched ')'");
			}
			else if (type == COMMENT)
				done = FALSE;
			else if (type == TICK)
			{
				error(1,0,"Invalid symbol");
			}
		}
	}
	return cmd;
}

/* Make a command stream given file handler */
command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
	int num_cmds = 0;
	int* line = checked_malloc(sizeof(int));
	*line = 0;
	char *line_buffer = checked_malloc(BUFFER_SIZE);
	command_stream_t stream = checked_malloc(sizeof(struct command_stream));
	stream->list = checked_malloc(sizeof(struct command_node));
	struct command_node* walk = stream->list;

	//while ((walk->cmd = make_command(get_next_byte,
				//get_next_byte_argument, line_buffer, BUFFER_SIZE, line)) != NULL) 
	while ((walk->cmd = make_command(get_next_byte,
				get_next_byte_argument, line_buffer, 0, line, 0)) != NULL)
	{
			walk->nxt = checked_malloc(sizeof(struct command_node));
			walk = walk->nxt;
			num_cmds++;
	}
	//error (1, 0, "command make command stream not yet implemented");
  return stream;
}

command_t
read_command_stream (command_stream_t s)
{
	command_t cmd = s->list->cmd;
	struct command_node* temp = s->list;
	if (s->list != NULL)
		s->list = s->list->nxt;
  //error (1, 0, "command reading not yet implemented");
	free(temp);
  return cmd;
}
