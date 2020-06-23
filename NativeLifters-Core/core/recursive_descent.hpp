// Copyright (c) 2020 Can Boluk and contributors of the VTIL Project   
// All rights reserved.   
//    
// Redistribution and use in source and binary forms, with or without   
// modification, are permitted provided that the following conditions are met: 
//    
// 1. Redistributions of source code must retain the above copyright notice,   
//    this list of conditions and the following disclaimer.   
// 2. Redistributions in binary form must reproduce the above copyright   
//    notice, this list of conditions and the following disclaimer in the   
//    documentation and/or other materials provided with the distribution.   
// 3. Neither the name of mosquitto nor the names of its   
//    contributors may be used to endorse or promote products derived from   
//    this software without specific prior written permission.   
//    
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE   
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE  
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE   
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR   
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF   
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN   
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)   
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE  
// POSSIBILITY OF SUCH DAMAGE.        
//
#pragma once
#include <unordered_set>
#include <deque>
#include <vtil/arch>
#include <vtil/compiler>

namespace vtil::lifter
{
	// Generic recursive descent parser used for exploring control flow.
	//
	template<typename Input, typename Arch>
	struct recursive_descent
	{
		// Input descriptor.
		//
		Input* input;

		// Entry block.
		//
		basic_block* entry;

		// Instructions corresponding to their basic blocks.
		//
		std::unordered_map<uint64_t, basic_block*> leaders;

		// Start recursive descent.
		//
		void populate( basic_block* start_block )
		{
			// While the basic block is not complete, populate with instructions.
			//
			uint64_t vip = start_block->entry_vip;
			uint8_t* entry_ptr = input->get_at( vip );

			leaders.emplace( vip, start_block );

			while ( true)
			{
				if ( !input->is_valid( vip ) )
				{
					start_block->vexit( invalid_vip );
					return;
				}

				auto offs = Arch::process( start_block, vip, entry_ptr );

				if ( start_block->is_complete() )
					break;

				entry_ptr += offs;
				vip += offs;
				
				if ( auto ldr = leaders.find( vip ); ldr != leaders.cend( ) )
				{
					start_block
						->jmp( vip )
						->fork( vip );

					break;
				}
			}

			// Run local optimizer passes on this block.
			//
			optimizer::apply_all( start_block );

			// Try to explore branches.
			//
			cached_tracer local_tracer = {};
			auto lbranch_info = optimizer::aux::analyze_branch( start_block, &local_tracer, true, true );

			for ( auto branch : lbranch_info.destinations )
			{
				if ( branch.is_constant( ) )
				{
					const auto branch_imm = *branch.get<uint64_t>( );
					if ( auto next_blk = start_block->fork( branch_imm ) )
					{
						populate( next_blk );
					}
				}
			}
		}

		recursive_descent( Input* input, uint64_t entry_point ) : input(input), leaders( { } )
		{
			entry = basic_block::begin( entry_point );
		}
	};
}