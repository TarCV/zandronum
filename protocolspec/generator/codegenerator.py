#!/usr/bin/env python3
# coding: utf-8
#
#	Copyright 2016 Teemu Piippo
#	All rights reserved.
#
#	Redistribution and use in source and binary forms, with or without
#	modification, are permitted provided that the following conditions
#	are met:
#
#	1. Redistributions of source code must retain the above copyright
#	   notice, this list of conditions and the following disclaimer.
#	2. Redistributions in binary form must reproduce the above copyright
#	   notice, this list of conditions and the following disclaimer in the
#	   documentation and/or other materials provided with the distribution.
#	3. Neither the name of the copyright holder nor the names of its
#	   contributors may be used to endorse or promote products derived from
#	   this software without specific prior written permission.
#
#	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
#	TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
#	PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
#	OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#	EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
#	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#	PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING
#	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

class SourceCodeWriter:
	'''
		Base class for SourceWriter and HeaderWriter. Provides useful methods for writing source code with.
	'''
	def __init__(self, output, spec):
		self.output = output
		self.tabs = ''
		self.writingsender = False
		self.spec = spec
		super().__init__()

	def getcommands(self, protocolName):
		'''
			Returns the commands of a specific protocol
		'''
		for command in self.spec.protocols[protocolName]['commands'].values():
			yield command

	def writeline(self, line, section=-1):
		'''
			Writes a single line into the source
		'''
		if line:
			self.output.write(self.tabs + line + '\n', section = section)
		else:
			self.output.write('\n', section = section)

	def declare(self, typename, varname):
		'''
			Writes code to declare a variable into the current declarations section.
		'''
		self.writeline('{typename} {varname};'.format(**locals()), section = self.declsection)

	def indent(self):
		'''
			Adds a level of indent.
		'''
		self.tabs += '\t'

	def unindent(self):
		'''
			Removes one level of indent.
		'''
		self.tabs = self.tabs[:-1]

	def startscope(self):
		'''
			Convenience function, adds a starting brace and increases indentation.
		'''
		self.writeline('{')
		self.indent()

	def endscope(self):
		'''
			Convenience function, decreases indentation and adds a closing brace.
		'''
		self.unindent()
		self.writeline('}')

	def writecontext(self, context):
		'''
			Writes a context, i.e. a here document containing source code into the file.

			Indentation containing in the heredoc's first line is removed from all lines (so all indentation
			is considered relative to the first line), and the source file's own indentation is added to all lines.
		'''
		# Find out what indentation this heredoc has
		from re import search
		context = context.lstrip('\n')
		indentation = search(r'^(\s*)', context).group(1)

		# Write each line of the heredoc, subtracting the indentation found in the first line.
		for line in context.splitlines():
			if line.startswith(indentation):
				line = line[len(indentation):]
			self.writeline(line)

class SourceWriter(SourceCodeWriter):
	'''
		Generates the servercommands.cpp source file.
	'''
	def __init__(self, output, spec):
		super().__init__(output, spec)
		from itertools import count
		self.activecondition = ''

		# This beauty is a generator that returns temporary variable names.
		self.tempvar = ('temp%d' % i for i in count())

	def write(self):
		'''
			Writes the servercommands.cpp source file.
		'''
		self.writeline('#include "cl_main.h"')
		self.writeline('#include "servercommands.h"')
		self.writeline('#include "network.h"')
		self.writeline('#include "network/netcommand.h"')
		self.writeline('')
		self.beginfunction(name = 'CLIENT_ParseServerCommand', enumtype = 'SVC')

		for command in self.getcommands('GameServerToClient'):
			if not command.extended:
				self.writecommandreader(command)

		self.endfunction()
		self.writeline('')
		self.beginfunction(name = 'CLIENT_ParseExtendedServerCommand', enumtype = 'SVC2')

		# Write handling for the commands in the source:
		for command in self.getcommands('GameServerToClient'):
			if command.extended:
				self.writecommandreader(command)

		self.endfunction()

		for command in self.getcommands('GameServerToClient'):
			# Write the SendToClient methods
			self.writesender(command)

			# Write setter methods
			for i, parameter in enumerate(command):
				self.writeline('')
				self.writecontext('''
					void ServerCommands::{commandname}::{setter}( {typename} value )
					{{
						this->{parametername} = value;
						this->_verifyTable[{i}] = true;
					}}'''.format(commandname = command.name,
							     setter = parameter.setter,
							     typename = parameter.constreference,
							     parametername = parameter.name,
							     i = i))

			# Write the condition check methods. The items must be sorted first or the methods will be written in a
			# different order each time, causing unnecessary writes.
			self.writeline('')
			for condition, methodname in sorted(command.conditionchecknames.items()):
				self.writecontext('''
					bool ServerCommands::{commandname}::{methodname}() const
					{{
						return ( {condition} );
					}}
					'''.format(commandname = command.name, **locals()))

	def beginfunction(self, name, enumtype):
		'''
			Writes the beginning of a function definition.
		'''
		self.writeline('bool {name}( {enumtype} header, BYTESTREAM_s *bytestream )'.format(**locals()))
		self.startscope()
		self.writeline('switch ( header )')
		self.writeline('{')

	def endfunction(self):
		'''
			Writes the end of a function definition
		'''
		self.writeline('default:')
		self.writeline('\treturn false;')
		self.writeline('}')
		self.endscope()

	def writeparameter(self, command, parameter, parametername):
		parameter.type.writeread(self, command, parameter, parametername)

	def writecommandreader(self, command):
		'''
			Writes code to handle the read of a server command.
		'''
		commandname = command.name
		self.writeline('case %s:' % command.enumname)
		self.indent()
		self.startscope()

		self.declsection = self.output.addsection(command.name + ' declarations')
		self.readsection = self.output.addsection(command.name + ' read')
		self.checksection = self.output.addsection(command.name + ' checks')
		self.output.setcurrentsection(self.declsection)
		# Write reading code and check parameter-specific conditions
		self.writeline('ServerCommands::%s command;' % command.name)
		self.output.setcurrentsection(self.readsection)
		self.handleparameters(command, 'writeread')
		# The checks need to be added separately so that everything can be read first, and only then we start
		# doing any checks.
		self.output.setcurrentsection(self.checksection)
		self.handleparameters(command, 'writereadchecks')

		# Generate code to print a warning if the packet is too short, and to execute the command if all is OK, unless
		# the command was empty (in which case we're not reading anything).
		if command.parameters:
			self.writecontext('''
			if ( bytestream->pbStream > bytestream->pbStreamEnd )
			{{
				CLIENT_PrintWarning( "{commandname}: Packet contained %td too few bytes\\n",
					bytestream->pbStream - bytestream->pbStreamEnd );
				return true;
			}}
			'''.format(**locals()))

		# If all is good, then execute the command
		self.output.setcurrentsection(self.output.addsection(command.name + ' finish'))
		self.writeline('command.Execute();')
		self.endscope()
		self.writeline('return true;')
		self.unindent()
		self.writeline('')

	def handleparameters(self, command, methodname):
		'''
			Writes code to handle a parameter.
			The method is expected to take the arguments:
				- writer (this object)
				- command
				- parametername -- this is a reference to the actual parameter object (makes a difference with arrays).
			The method is expected to be a method of a SpecParameter subclass, and is expected to write code handling
			recievement or sending of the parameter in question.
		'''
		for parameter in command:
			# If the condition for this parameter changed, we need to handle that:
			if parameter.condition != self.activecondition:
				# If there was a condition, the if block needs to be closed before a new one can begin.
				if self.activecondition:
					self.endscope()

				# If we have a condition now, add an if block for it.
				if parameter.condition:
					functionname = command.parameterchecknames[parameter.name]

					# If we're not in a method of this command, we need to call it a little differently
					if not self.writingsender:
						functionname = 'command.' + functionname

					self.writeline('if ( %s() )' % functionname)
					self.startscope()

				# Mark down what our current condition is.
				self.activecondition = parameter.condition

			if not parameter.isarray:
				# If it's not an array, just call the method
				getattr(parameter, methodname)(writer = self, command = command, parametername = parameter.name)
			else:
				# Otherwise, we'll need to treat it a bit differently:
				if self.writingsender:
					# If we're writing the sending code, write array's size into the command, and use it to loop over
					# the elements.
					self.writeline('command.addByte( %s.Size() );' % parameter.name)
					self.writeline('for ( unsigned int i = 0; i < %s.Size(); ++i )' % parameter.name)
				else:
					# If we're writing the reading code, use a size variable.
					if not hasattr(parameter, 'sizevariable'):
						# If it doesn't exist yet, create it now.
						parameter.sizevariable = next(self.tempvar)
						self.declare('unsigned int', parameter.sizevariable)

						# Since we're initializing the size variable, read in its value, and use it to allocate
						# the array.
						self.writeline('%s = NETWORK_ReadByte( bytestream );' % parameter.sizevariable)
						self.writeline('command.%s.Reserve( %s );' % (parameter.name, parameter.sizevariable))

					# Now use it to iterate the array for per-element code.
					self.writeline('for ( unsigned int i = 0; i < %s; ++i )' % parameter.sizevariable)

				# In any case, we have now written a for loop. Now, we begin a scope, and write the iteration.
				# The parameter name gets "[i]" appended, so that it references the correct element, instead of the
				# array itself.
				self.startscope()
				getattr(parameter, methodname)(writer = self, command = command, parametername = parameter.name + '[i]')
				self.endscope()

		# If we're still in an if-block, close it now.
		if self.activecondition:
			self.endscope()
			self.activecondition = ''

	def writesender(self, command):
		'''
			Generates the sendCommandToClients method of a ServerCommand
		'''
		commandname = command.name
		enumname = command.enumname

		# Write the function signature.
		self.writecontext('void ServerCommands::{commandname}::sendCommandToClients( int playerExtra, ServerCommandFlags flags )'.format(**locals()))
		self.writingsender = True
		self.startscope()

		# Check that all the parameters were provided. If not, print a warning.
		if command.parameters:
			test = ' || '.join('_verifyTable[%d] == false' % i for i in range(len(command.parameters)))
			self.writeline('if ( %s )' % test)
			self.startscope()
			self.writeline('Printf( "WARNING: {commandname}::sendCommandToClients: not all parameters were initialized:\\n" );'.format(**locals()))

			# Be more specific about which parameters in particular were left out.
			for i, parameter in enumerate(command.parameters.keys()):
				line = 'if ( _verifyTable[{i}] == false ) Printf( "Missing: {name}\\n" );'
				line = line.format(i = i, name = parameter)
				self.writeline(line)
			self.endscope()

		self.writeline('NetCommand command ( {enumname} );'''.format(**locals()))

		# If this command is unreliabe, write that down.
		if command.unreliable:
			self.writeline('command.setUnreliable( true );')

		# Let parameters write their senders in.
		self.handleparameters(command, 'writesend')

		# Write the code to send this command, and finish the function.
		self.writeline('command.sendCommandToClients( playerExtra, flags );')
		self.endscope()
		self.writingsender = False

class HeaderWriter(SourceCodeWriter):
	'''
		Generates the servercommands.h file.
	'''
	def write(self):
		'''
			Writes the header file
		'''

		self.writeline('#pragma once')

		# Include a few important header files we know we'll need
		self.writeline('#include "actor.h"')
		self.writeline('#include "zstring.h"')
		self.writeline('#include "network_enums.h"')
		self.writeline('#include "networkshared.h"')
		self.writeline('#include "sv_commands.h"')
		self.writeline('#include "joinqueue.h"')

		# Add any includes from the spec
		for include in self.spec.includes:
			self.writeline('#include "%s"' % include)

		# Write in the signatures for our parsing functions.
		self.writeline('')
		self.writeline('bool CLIENT_ParseServerCommand( SVC header, BYTESTREAM_s *bytestream );')
		self.writeline('bool CLIENT_ParseExtendedServerCommand( SVC2 header, BYTESTREAM_s *bytestream );')
		self.writeline('')

		# Add a namespace, so that we don't pollute the global namespace with the server commands.
		self.writeline('namespace ServerCommands')
		self.startscope()

		# Write down the defintions for all structures.
		for struct in self.spec.protocols['GameServerToClient']['structs'].values():
			self.writeline('struct %s' % struct['name'])
			self.writeline('{')
			self.indent()
			for name, parameter in struct['members'].items():
				self.writeline('{type} {name};'.format(
					type = parameter.cxxtypename,
					name = name,
				))
			self.unindent()
			self.writeline('};')
			self.writeline('')

		# For each server command, create a class to represent it.
		for command in self.getcommands('GameServerToClient'):
			self.writeline('class %s' % command.name)
			self.writeline('{')
			self.writeline('public:')
			self.indent()

			# We need to initialize the verification table to zero, if there is one.
			if command.parameters:
				self.writeline('%s() { memset( _verifyTable, 0, sizeof _verifyTable ); }' % command.name)

			# Add setter methods for each parameter
			for parameter in command:
				self.writeline('void %s( %s value );' % (parameter.setter, parameter.constreference))

			# Add condition check methods. The values must be sorted first, or the order of the methods changes
			# every time the header is generated, which causes the header to change every time this script is run and
			# we don't really want that.
			for name in sorted(command.conditionchecknames.values()):
				self.writeline('bool %s() const;' % name)

			# Add the Execute() method, that cl_main.cpp will define.
			self.writeline('void Execute();')

			# Add the sendCommandToClients() method for sv_commands.cpp
			self.writeline('void sendCommandToClients( int playerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );')

			# The parser function must be a friend of this command, so that it can fill in parameters.
			if command.extended:
				self.writeline('friend bool ::CLIENT_ParseExtendedServerCommand( SVC2, BYTESTREAM_s * );')
			else:
				self.writeline('friend bool ::CLIENT_ParseServerCommand( SVC, BYTESTREAM_s * );')

			# Fill in the parameters into a private section.
			self.unindent()
			self.writeline('')
			self.writeline('private:')
			self.indent()
			for parameter in command:
				self.define(parameter)

			# Add an array of bools, so that we can checking whether all members are initialized when the command
			# is launched. Unless the command has no parameters, of course.
			if command.parameters:
				self.writeline('bool _verifyTable[%d];' % len(command.parameters))

			# Done with this structure, now close it.
			self.unindent()
			self.writeline('};')
			self.writeline('')

		self.endscope()

	def define(self, parameter):
		'''
			Creates a variable definition for a SpecParameter
		'''
		definition = parameter.cxxtypename

		# Check that short bytes have the proper amount of bits set.
		if parameter.typename == 'shortbyte':
			try:
				assert int(parameter.specialization) in range(1, 8 + 1) # [1…8]
			except:
				raise Exception('Short bytes must be specialized to a number in range 1…8')

		# Allow strings to be specialized into FNames
		if definition == 'FString' and parameter.specialization:
			if parameter.specialization == 'Name':
				definition = 'FName'
			else:
				raise Exception('String parameters can only be specialized to Name')

		# If this is an array, encapsulate it into a TArray.
		if parameter.isarray:
			definition = 'TArray<%s>' % definition

		# Add a space, unless this is a pointer or reference type
		if not definition.endswith(' *'):
			definition += ' '

		# Add the name of the parameter
		definition += parameter.name + ';'
		self.writeline(definition)

def main():
	# Parse the command line arguments.
	from argparse import ArgumentParser
	argparser = ArgumentParser()
	argparser.add_argument('--spec', required = True)
	argparser.add_argument('--source', required = True)
	argparser.add_argument('--header', required = True)
	args = argparser.parse_args()

	# Parse the specification file
	from spec import NetworkSpec
	spec = NetworkSpec()

	try:
		spec.loadfromfile(args.spec)
	except Exception as error:
		# Got an error. Find out where it came from and print an IDE-friendly message to stderr.
		from sys import stderr
		print('{filename}:{linenumber}: error: {errormessage}'.format(
			filename = spec.currentfilename,
			linenumber = spec.linenumber,
			errormessage = type(error) == Exception and str(error) or ('[%s] %s' % (type(error).__name__, error)),
		), file = stderr)
		return 1
	else:
		# Generate the header and source files
		from outputfile import OutputFile
		header = OutputFile(args.header)
		source = OutputFile(args.source)
		HeaderWriter(header, spec).write()
		SourceWriter(source, spec).write()
		header.save()
		source.save()
		return 0

if __name__ == '__main__':
	exit(main())
