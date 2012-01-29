/*!
 * \file correlator.h
 * \brief Highly optimized vector correlator class
 * \author Javier Arribas, 2011. jarribas(at)cttc.es
 *
 * Class that implements a high optimized vector correlator class.
 *
 * -------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2012  (see AUTHORS file for a list of contributors)
 *
 * GNSS-SDR is a software defined Global Navigation
 *          Satellite Systems receiver
 *
 * This file is part of GNSS-SDR.
 *
 * GNSS-SDR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * at your option) any later version.
 *
 * GNSS-SDR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNSS-SDR. If not, see <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------
 */

#include <iostream>
#include <gnuradio/gr_block.h>
#include "correlator.h"

unsigned long Correlator::next_power_2(unsigned long v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}




void Correlator::Carrier_wipeoff_and_EPL_generic(int signal_length_samples,const gr_complex* input, gr_complex* carrier,gr_complex* E_code, gr_complex* P_code, gr_complex* L_code,gr_complex* E_out, gr_complex* P_out, gr_complex* L_out)
{
    gr_complex bb_signal_sample(0,0);

    //std::cout<<"length="<<signal_length_samples<<std::endl;

    *E_out=0;
    *P_out=0;
    *L_out=0;
    // perform Early, Prompt and Late correlation
    for(int i=0; i<signal_length_samples; ++i)
        {
            //Perform the carrier wipe-off
            bb_signal_sample = input[i] * carrier[i];
            // Now get early, late, and prompt values for each
            *E_out += bb_signal_sample * E_code[i];
            *P_out += bb_signal_sample * P_code[i];
            *L_out += bb_signal_sample * L_code[i];
        }
}




void Correlator::Carrier_wipeoff_and_EPL_volk(int signal_length_samples,const gr_complex* input, gr_complex* carrier,gr_complex* E_code, gr_complex* P_code, gr_complex* L_code,gr_complex* E_out, gr_complex* P_out, gr_complex* L_out)
{
    gr_complex* bb_signal;
    gr_complex* input_aligned;
    //gr_complex* carrier_aligned;

    // signal_length_samples=next_power_2(signal_length_samples);
    //std::cout<<"length="<<signal_length_samples<<std::endl;

    //long int new_length=next_power_2(signal_length_samples);
    //todo: do something if posix_memalign fails
    if (posix_memalign((void**)&bb_signal, 16, signal_length_samples * sizeof(gr_complex)) == 0) {};
    if (posix_memalign((void**)&input_aligned, 16, signal_length_samples * sizeof(gr_complex)) == 0){};
    //posix_memalign((void**)&carrier_aligned, 16, new_length*sizeof(gr_complex));

    memcpy(input_aligned,input,signal_length_samples * sizeof(gr_complex));
    //memcpy(carrier_aligned,carrier,signal_length_samples*sizeof(gr_complex));

    volk_32fc_x2_multiply_32fc_a_manual(bb_signal, input_aligned, carrier, signal_length_samples,  volk_32fc_x2_multiply_32fc_a_best_arch.c_str());
    volk_32fc_x2_dot_prod_32fc_a_manual(E_out, bb_signal, E_code, signal_length_samples * sizeof(gr_complex),  volk_32fc_x2_dot_prod_32fc_a_best_arch.c_str());
    volk_32fc_x2_dot_prod_32fc_a_manual(P_out, bb_signal, P_code, signal_length_samples * sizeof(gr_complex),  volk_32fc_x2_dot_prod_32fc_a_best_arch.c_str());
    volk_32fc_x2_dot_prod_32fc_a_manual(L_out, bb_signal, L_code, signal_length_samples * sizeof(gr_complex),  volk_32fc_x2_dot_prod_32fc_a_best_arch.c_str());

    free(bb_signal);
    free(input_aligned);
    //free(carrier_aligned);
}




void Correlator::cpu_arch_test_volk_32fc_x2_dot_prod_32fc_a()
{
    //
    struct volk_func_desc desc=volk_32fc_x2_dot_prod_32fc_a_get_func_desc();
    std::vector<std::string> arch_list;

    for(int i = 0; i < desc.n_archs; ++i)
        {
            //if(!(archs[i+1] & volk_get_lvarch())) continue; //this arch isn't available on this pc
            arch_list.push_back(std::string(desc.indices[i]));
        }


    //first let's get a list of available architectures for the test
    if(arch_list.size() < 2)
        {
            std::cout << "no architectures to test" << std::endl;
            this->volk_32fc_x2_dot_prod_32fc_a_best_arch = "generic";
        }
    else
        {
            std::cout << "Detected architectures in this machine for volk_32fc_x2_dot_prod_32fc_a:" << std::endl;
            for (unsigned int i=0; i<arch_list.size(); ++i)
                {
                    std::cout << "Arch " << i << ":" << arch_list.at(i) << std::endl;
                }
            // TODO: Make a test to find the best architecture
            this->volk_32fc_x2_dot_prod_32fc_a_best_arch = arch_list.at(arch_list.size() - 1);
        }
    std::cout<<"Selected architecture for volk_32fc_x2_dot_prod_32fc_a is "<<this->volk_32fc_x2_dot_prod_32fc_a_best_arch<<std::endl;
}






void Correlator::cpu_arch_test_volk_32fc_x2_multiply_32fc_a()
{
    //
    struct volk_func_desc desc = volk_32fc_x2_multiply_32fc_a_get_func_desc();
    std::vector<std::string> arch_list;

    for(int i = 0; i < desc.n_archs; ++i)
        {
            //if(!(archs[i+1] & volk_get_lvarch())) continue; //this arch isn't available on this pc
            arch_list.push_back(std::string(desc.indices[i]));
        }

    this->volk_32fc_x2_multiply_32fc_a_best_arch = "generic";
    //first let's get a list of available architectures for the test
    if(arch_list.size() < 2)
        {
            std::cout << "no architectures to test" << std::endl;
        }
    else
        {
            std::cout << "Detected architectures in this machine for volk_32fc_x2_multiply_32fc_a:" << std::endl;
            for (unsigned int i=0; i < arch_list.size(); ++i)
                {
                    std::cout << "Arch " << i << ":" << arch_list.at(i) << std::endl;
                    if (arch_list.at(i).find("sse")!=std::string::npos)
                        {
                            // TODO: Make a test to find the best architecture
                            this->volk_32fc_x2_multiply_32fc_a_best_arch = arch_list.at(i);
                        }
                }
        }
    std::cout<<"Selected architecture for volk_32fc_x2_multiply_32fc_a_best_arch is "<<this->volk_32fc_x2_multiply_32fc_a_best_arch<<std::endl;
}




Correlator::Correlator ()
{
    cpu_arch_test_volk_32fc_x2_dot_prod_32fc_a();
    cpu_arch_test_volk_32fc_x2_multiply_32fc_a();
}





Correlator::~Correlator ()
{}
