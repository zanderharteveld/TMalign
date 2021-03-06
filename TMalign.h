/*
===============================================================================
   Implementation of TM-align in C/C++   

   This program is written by Jianyi Yang at
   Yang Zhang lab
   And it is updated by Jianjie Wu at
   Yang Zhang lab
   Department of Computational Medicine and Bioinformatics 
   University of Michigan 
   100 Washtenaw Avenue, Ann Arbor, MI 48109-2218 
           
   Please report bugs and questions to jianjiew@umich.edu or zhng@umich.edu
===============================================================================
*/
#include "basic_define.h"
#include <iomanip>

void print_version()
{
    cout << 
"\n"
" *****************************************************************************\n"
" * TM-align (Version "<< TMalign_version <<
"): A protein structural alignment algorithm     *\n"
" * Reference: Y Zhang and J Skolnick, Nucl Acids Res 33, 2302-9 (2005)       *\n"
" * Please email your comments and suggestions to Yang Zhang (zhng@umich.edu) *\n"
" *****************************************************************************"
    << endl;
}

int load_PDB_allocate_memory(const char *xname, const char *yname,
    vector<string> &PDB_lines1, vector<string> &PDB_lines2,
    const int ter_opt=3, const string atom_opt=" CA ")
{
    tempxlen=PDB_lines1.size();
    tempylen=PDB_lines2.size();
    if (!tempxlen) tempxlen=get_PDB_lines(xname,PDB_lines1,ter_opt,atom_opt);
    if (!tempxlen) return 1; // fail to read chain1
    if (!tempylen) tempylen=get_PDB_lines(yname,PDB_lines2,ter_opt,atom_opt);
    if (!tempylen) return 2; // fail to read chain2

    //------allocate memory for x and y------>
    NewArray(&xa, tempxlen, 3);
    seqx = new char[tempxlen + 1];
    secx = new int[tempxlen];
    xresno = new int[tempxlen];

    NewArray(&ya, tempylen, 3);
    seqy = new char[tempylen + 1];
    yresno = new int[tempylen];
    secy = new int[tempylen];

    // Get exact length
    xlen = read_PDB(PDB_lines1, xa, seqx, xresno);
    ylen = read_PDB(PDB_lines2, ya, seqy, yresno);
    minlen = min(xlen, ylen);
    
    //------allocate memory for other temporary varialbes------>
    NewArray(&r1, minlen, 3);
    NewArray(&r2, minlen, 3);
    NewArray(&xtm, minlen, 3);
    NewArray(&ytm, minlen, 3);
    NewArray(&xt, xlen, 3);

    NewArray(&score, xlen+1, ylen+1);
    NewArray(&path, xlen+1, ylen+1);
    NewArray(&val, xlen+1, ylen+1);  
    return 0; // 0 for no error
}


void free_memory()
{
    DeleteArray(&path, xlen+1);
    DeleteArray(&val, xlen+1);
    DeleteArray(&score, xlen+1);
    DeleteArray(&xa, tempxlen);
    DeleteArray(&xt, xlen);
    DeleteArray(&ya, tempylen);
    DeleteArray(&r1, minlen);
    DeleteArray(&r2, minlen);
    DeleteArray(&xtm, minlen);
    DeleteArray(&ytm, minlen);
   
    delete [] seqx;
    delete [] seqy;
    delete [] secx;
    delete [] secy;
    delete [] xresno;
    delete [] yresno;
}


//     1, collect those residues with dis<d;
//     2, calculate TMscore
int score_fun8( double **xa, 
                double **ya, 
                int n_ali,
                double d,
                int i_ali[], 
                double *score1,
                int score_sum_method
              )
{
    double score_sum=0, di;
    double d_tmp=d*d;
    double d02=d0*d0;
    double score_d8_cut = score_d8*score_d8;
    
    int i, n_cut, inc=0;

    while(1)
    {
        n_cut=0;
        score_sum=0;
        for(i=0; i<n_ali; i++)
        {
            di = dist(xa[i], ya[i]);
            if(di<d_tmp)
            {
                i_ali[n_cut]=i;
                n_cut++;
            }
            if(score_sum_method==8)
            {                
                if(di<=score_d8_cut)
                {                    
                    score_sum += 1/(1+di/d02);
                }                
            }
            else
            {
                score_sum += 1/(1+di/d02);
            }
        }
        //there are not enough feasible pairs, reliefe the threshold         
        if(n_cut<3 && n_ali>3)
        {
            inc++;
            double dinc=(d+inc*0.5);
            d_tmp = dinc * dinc;
        }
        else
        {
            break;
        }

    }  

    *score1=score_sum/Lnorm;
    
    return n_cut;
}

int score_fun8_standard(double **xa,
    double **ya,
    int n_ali,
    double d,
    int i_ali[],
    double *score1,
    int score_sum_method
    )
{
    double score_sum = 0, di;
    double d_tmp = d*d;
    double d02 = d0*d0;
    double score_d8_cut = score_d8*score_d8;

    int i, n_cut, inc = 0;
    while (1)
    {
        n_cut = 0;
        score_sum = 0;
        for (i = 0; i<n_ali; i++)
        {
            di = dist(xa[i], ya[i]);
            if (di<d_tmp)
            {
                i_ali[n_cut] = i;
                n_cut++;
            }
            if (score_sum_method == 8)
            {
                if (di <= score_d8_cut)
                {
                    score_sum += 1 / (1 + di / d02);
                }
            }
            else
            {
                score_sum += 1 / (1 + di / d02);
            }
        }
        //there are not enough feasible pairs, reliefe the threshold         
        if (n_cut<3 && n_ali>3)
        {
            inc++;
            double dinc = (d + inc*0.5);
            d_tmp = dinc * dinc;
        }
        else
        {
            break;
        }

    }

    *score1 = score_sum / n_ali;
    return n_cut;
}

double TMscore8_search( double **xtm, 
                        double **ytm,
                        int Lali, 
                        double t0[3],
                        double u0[3][3],
                        int simplify_step,
                        int score_sum_method,
                        double *Rcomm,
                        double local_d0_search
                       )
{ 
    int i, m;
    double score_max, score, rmsd;    
    const int kmax=Lali;    
    int k_ali[kmax], ka, k;
    double t[3];
    double u[3][3];
    double d;
    

    //iterative parameters
    int n_it=20;            //maximum number of iterations
    int n_init_max=6; //maximum number of different fragment length 
    int L_ini[n_init_max];  //fragment lengths, Lali, Lali/2, Lali/4 ... 4   
    int L_ini_min=4;
    if(Lali<L_ini_min) L_ini_min=Lali;   

    int n_init=0, i_init;      
    for(i=0; i<n_init_max-1; i++)
    {
        n_init++;
        L_ini[i]=(int) (Lali/pow(2.0, (double) i));
        if(L_ini[i]<=L_ini_min)
        {
            L_ini[i]=L_ini_min;
            break;
        }
    }
    if(i==n_init_max-1)
    {
        n_init++;
        L_ini[i]=L_ini_min;
    }
    
    score_max=-1;
    //find the maximum score starting from local structures superposition
    int i_ali[kmax], n_cut;
    int L_frag; //fragment length
    int iL_max; //maximum starting postion for the fragment
    
    for(i_init=0; i_init<n_init; i_init++)
    {
        L_frag=L_ini[i_init];
        iL_max=Lali-L_frag;
      
        i=0;   
        while(1)
        {
            //extract the fragment starting from position i 
            ka=0;
            for(k=0; k<L_frag; k++)
            {
                int kk=k+i;
                r1[k][0]=xtm[kk][0];  
                r1[k][1]=xtm[kk][1]; 
                r1[k][2]=xtm[kk][2];   
                
                r2[k][0]=ytm[kk][0];  
                r2[k][1]=ytm[kk][1]; 
                r2[k][2]=ytm[kk][2];
                
                k_ali[ka]=kk;
                ka++;
            }
            
            //extract rotation matrix based on the fragment
            Kabsch(r1, r2, L_frag, 1, &rmsd, t, u);
            if (simplify_step != 1)
                *Rcomm = 0;
            do_rotation(xtm, xt, Lali, t, u);
            
            //get subsegment of this fragment
            d = local_d0_search - 1;
            n_cut=score_fun8(xt, ytm, Lali, d, i_ali, &score, score_sum_method);
            if(score>score_max)
            {
                score_max=score;
                
                //save the rotation matrix
                for(k=0; k<3; k++)
                {
                    t0[k]=t[k];
                    u0[k][0]=u[k][0];
                    u0[k][1]=u[k][1];
                    u0[k][2]=u[k][2];
                }
            }
            
            //try to extend the alignment iteratively            
            d = local_d0_search + 1;
            for(int it=0; it<n_it; it++)            
            {
                ka=0;
                for(k=0; k<n_cut; k++)
                {
                    m=i_ali[k];
                    r1[k][0]=xtm[m][0];  
                    r1[k][1]=xtm[m][1]; 
                    r1[k][2]=xtm[m][2];
                    
                    r2[k][0]=ytm[m][0];  
                    r2[k][1]=ytm[m][1]; 
                    r2[k][2]=ytm[m][2];
                    
                    k_ali[ka]=m;
                    ka++;
                } 
                //extract rotation matrix based on the fragment                
                Kabsch(r1, r2, n_cut, 1, &rmsd, t, u);
                do_rotation(xtm, xt, Lali, t, u);
                n_cut=score_fun8(xt, ytm, Lali, d, i_ali, &score, score_sum_method);
                if(score>score_max)
                {
                    score_max=score;

                    //save the rotation matrix
                    for(k=0; k<3; k++)
                    {
                        t0[k]=t[k];
                        u0[k][0]=u[k][0];
                        u0[k][1]=u[k][1];
                        u0[k][2]=u[k][2];
                    }                     
                }
                
                //check if it converges            
                if(n_cut==ka)
                {                
                    for(k=0; k<n_cut; k++)
                    {
                        if(i_ali[k]!=k_ali[k])
                        {
                            break;
                        }
                    }
                    if(k==n_cut)
                    {                        
                        break; //stop iteration
                    }
                }                                                               
            } //for iteration            

            if(i<iL_max)
            {
                i=i+simplify_step; //shift the fragment        
                if(i>iL_max) i=iL_max;  //do this to use the last missed fragment
            }
            else if(i>=iL_max)
            {
                break;
            }
        }//while(1)
        //end of one fragment
    }//for(i_init
    return score_max;
}


double TMscore8_search_standard(double **xtm,
    double **ytm,
    int Lali,
    double t0[3],
    double u0[3][3],
    int simplify_step,
    int score_sum_method,
    double *Rcomm,
    double local_d0_search
    )
{
    int i, m;
    double score_max, score, rmsd;
    const int kmax = Lali;
    int k_ali[kmax], ka, k;
    double t[3];
    double u[3][3];
    double d;


    //iterative parameters
    int n_it = 20;            //maximum number of iterations
    int n_init_max = 6; //maximum number of different fragment length 
    int L_ini[n_init_max];  //fragment lengths, Lali, Lali/2, Lali/4 ... 4   
    int L_ini_min = 4;
    if (Lali<L_ini_min) L_ini_min = Lali;

    int n_init = 0, i_init;
    for (i = 0; i<n_init_max - 1; i++)
    {
        n_init++;
        L_ini[i] = (int)(Lali / pow(2.0, (double)i));
        if (L_ini[i] <= L_ini_min)
        {
            L_ini[i] = L_ini_min;
            break;
        }
    }
    if (i == n_init_max - 1)
    {
        n_init++;
        L_ini[i] = L_ini_min;
    }

    score_max = -1;
    //find the maximum score starting from local structures superposition
    int i_ali[kmax], n_cut;
    int L_frag; //fragment length
    int iL_max; //maximum starting postion for the fragment

    for (i_init = 0; i_init<n_init; i_init++)
    {
        L_frag = L_ini[i_init];
        iL_max = Lali - L_frag;

        i = 0;
        while (1)
        {
            //extract the fragment starting from position i 
            ka = 0;
            for (k = 0; k<L_frag; k++)
            {
                int kk = k + i;
                r1[k][0] = xtm[kk][0];
                r1[k][1] = xtm[kk][1];
                r1[k][2] = xtm[kk][2];

                r2[k][0] = ytm[kk][0];
                r2[k][1] = ytm[kk][1];
                r2[k][2] = ytm[kk][2];

                k_ali[ka] = kk;
                ka++;
            }
            //extract rotation matrix based on the fragment
            Kabsch(r1, r2, L_frag, 1, &rmsd, t, u);
            if (simplify_step != 1)
                *Rcomm = 0;
            do_rotation(xtm, xt, Lali, t, u);

            //get subsegment of this fragment
            d = local_d0_search - 1;
            n_cut = score_fun8_standard(xt, ytm, Lali, d, i_ali, &score, score_sum_method);

            if (score>score_max)
            {
                score_max = score;

                //save the rotation matrix
                for (k = 0; k<3; k++)
                {
                    t0[k] = t[k];
                    u0[k][0] = u[k][0];
                    u0[k][1] = u[k][1];
                    u0[k][2] = u[k][2];
                }
            }

            //try to extend the alignment iteratively            
            d = local_d0_search + 1;
            for (int it = 0; it<n_it; it++)
            {
                ka = 0;
                for (k = 0; k<n_cut; k++)
                {
                    m = i_ali[k];
                    r1[k][0] = xtm[m][0];
                    r1[k][1] = xtm[m][1];
                    r1[k][2] = xtm[m][2];

                    r2[k][0] = ytm[m][0];
                    r2[k][1] = ytm[m][1];
                    r2[k][2] = ytm[m][2];

                    k_ali[ka] = m;
                    ka++;
                }
                //extract rotation matrix based on the fragment                
                Kabsch(r1, r2, n_cut, 1, &rmsd, t, u);
                do_rotation(xtm, xt, Lali, t, u);
                n_cut = score_fun8_standard(xt, ytm, Lali, d, i_ali, &score, score_sum_method);
                if (score>score_max)
                {
                    score_max = score;

                    //save the rotation matrix
                    for (k = 0; k<3; k++)
                    {
                        t0[k] = t[k];
                        u0[k][0] = u[k][0];
                        u0[k][1] = u[k][1];
                        u0[k][2] = u[k][2];
                    }
                }

                //check if it converges            
                if (n_cut == ka)
                {
                    for (k = 0; k<n_cut; k++)
                    {
                        if (i_ali[k] != k_ali[k])
                        {
                            break;
                        }
                    }
                    if (k == n_cut)
                    {
                        break; //stop iteration
                    }
                }
            } //for iteration            

            if (i<iL_max)
            {
                i = i + simplify_step; //shift the fragment        
                if (i>iL_max) i = iL_max;  //do this to use the last missed fragment
            }
            else if (i >= iL_max)
            {
                break;
            }
        }//while(1)
        //end of one fragment
    }//for(i_init
    return score_max;
}

//Comprehensive TMscore search engine
// input:   two vector sets: x, y
//          an alignment invmap0[] between x and y
//          simplify_step: 1 or 40 or other integers
//          score_sum_method: 0 for score over all pairs
//                            8 for socre over the pairs with dist<score_d8          
// output:  the best rotaion matrix t, u that results in highest TMscore
double detailed_search( double **x,
                        double **y, 
                        int x_len, 
                        int y_len, 
                        int invmap0[],
                        double t[3],
                        double u[3][3],
                        int simplify_step,
                        int score_sum_method,
                        double local_d0_search
                       )
{
    //x is model, y is template, try to superpose onto y
    int i, j, k;     
    double tmscore;
    double rmsd;



    k=0;
    for(i=0; i<y_len; i++) 
    {
        j=invmap0[i];
        if(j>=0) //aligned
        {
            xtm[k][0]=x[j][0];
            xtm[k][1]=x[j][1];
            xtm[k][2]=x[j][2];
                
            ytm[k][0]=y[i][0];
            ytm[k][1]=y[i][1];
            ytm[k][2]=y[i][2];
            k++;
        }
    }

    //detailed search 40-->1
    tmscore = TMscore8_search(xtm, ytm, k, t, u, simplify_step, score_sum_method, &rmsd, local_d0_search);
    return tmscore;
}

double detailed_search_standard(double **x,
                        double **y, 
                        int x_len, 
                        int y_len, 
                        int invmap0[],
                        double t[3],
                        double u[3][3],
                        int simplify_step,
                        int score_sum_method,
                        double local_d0_search,
                        const bool& bNormalize
                       )
{
    //x is model, y is template, try to superpose onto y
    int i, j, k;     
    double tmscore;
    double rmsd;



    k=0;
    for(i=0; i<y_len; i++) 
    {
        j=invmap0[i];
        if(j>=0) //aligned
        {
            xtm[k][0]=x[j][0];
            xtm[k][1]=x[j][1];
            xtm[k][2]=x[j][2];
                
            ytm[k][0]=y[i][0];
            ytm[k][1]=y[i][1];
            ytm[k][2]=y[i][2];
            k++;
        }
    }

    //detailed search 40-->1
    tmscore = TMscore8_search_standard(xtm, ytm, k, t, u, simplify_step, score_sum_method, &rmsd, local_d0_search);
    if (bNormalize)// "-i", to use standard_TMscore, then bNormalize=true, else bNormalize=false; 
        tmscore = tmscore * k / Lnorm;

    return tmscore;
}

//compute the score quickly in three iterations
double get_score_fast(double **x, double **y, int x_len, int y_len, int invmap[])
{
    double rms, tmscore, tmscore1, tmscore2;
    int i, j, k;

    k=0;
    for(j=0; j<y_len; j++)
    {
        i=invmap[j];
        if(i>=0)
        {
            r1[k][0]=x[i][0];
            r1[k][1]=x[i][1];
            r1[k][2]=x[i][2];

            r2[k][0]=y[j][0];
            r2[k][1]=y[j][1];
            r2[k][2]=y[j][2];
            
            xtm[k][0]=x[i][0];
            xtm[k][1]=x[i][1];
            xtm[k][2]=x[i][2];
            
            ytm[k][0]=y[j][0];
            ytm[k][1]=y[j][1];
            ytm[k][2]=y[j][2];                  
            
            k++;
        }
        else if(i!=-1)
        {
            PrintErrorAndQuit("Wrong map!\n");
        }       
    }
    Kabsch(r1, r2, k, 1, &rms, t, u);
    
    //evaluate score   
    double di;
    const int len=k;
    double dis[len];    
    double d00=d0_search;
    double d002=d00*d00;
    double d02=d0*d0;
    
    int n_ali=k;
    double xrot[3];
    tmscore=0;
    for(k=0; k<n_ali; k++)
    {
        transform(t, u, &xtm[k][0], xrot);        
        di=dist(xrot, &ytm[k][0]);
        dis[k]=di;
        tmscore += 1/(1+di/d02);
    }
    
   
   
   //second iteration 
    double d002t=d002;
    while(1)
    {
        j=0;
        for(k=0; k<n_ali; k++)
        {            
            if(dis[k]<=d002t)
            {
                r1[j][0]=xtm[k][0];
                r1[j][1]=xtm[k][1];
                r1[j][2]=xtm[k][2];
                
                r2[j][0]=ytm[k][0];
                r2[j][1]=ytm[k][1];
                r2[j][2]=ytm[k][2];
                
                j++;
            }
        }
        //there are not enough feasible pairs, relieve the threshold 
        if(j<3 && n_ali>3)
        {
            d002t += 0.5;
        }
        else
        {
            break;
        }
    }
    
    if(n_ali!=j)
    {
        Kabsch(r1, r2, j, 1, &rms, t, u);
        tmscore1=0;
        for(k=0; k<n_ali; k++)
        {
            transform(t, u, &xtm[k][0], xrot);        
            di=dist(xrot, &ytm[k][0]);
            dis[k]=di;
            tmscore1 += 1/(1+di/d02);
        }
        
        //third iteration
        d002t=d002+1;
       
        while(1)
        {
            j=0;
            for(k=0; k<n_ali; k++)
            {            
                if(dis[k]<=d002t)
                {
                    r1[j][0]=xtm[k][0];
                    r1[j][1]=xtm[k][1];
                    r1[j][2]=xtm[k][2];
                    
                    r2[j][0]=ytm[k][0];
                    r2[j][1]=ytm[k][1];
                    r2[j][2]=ytm[k][2];
                                        
                    j++;
                }
            }
            //there are not enough feasible pairs, relieve the threshold 
            if(j<3 && n_ali>3)
            {
                d002t += 0.5;
            }
            else
            {
                break;
            }
        }

        //evaluate the score
        Kabsch(r1, r2, j, 1, &rms, t, u);
        tmscore2=0;
        for(k=0; k<n_ali; k++)
        {
            transform(t, u, &xtm[k][0], xrot);
            di=dist(xrot, &ytm[k][0]);
            tmscore2 += 1/(1+di/d02);
        }    
    }
    else
    {
        tmscore1=tmscore;
        tmscore2=tmscore;
    }
    
      
    if(tmscore1>=tmscore) tmscore=tmscore1;
    if(tmscore2>=tmscore) tmscore=tmscore2;

    
    return tmscore; // no need to normalize this score because it will not be used for latter scoring
}


//perform gapless threading to find the best initial alignment
//input: x, y, x_len, y_len
//output: y2x0 stores the best alignment: e.g., 
//y2x0[j]=i means:
//the jth element in y is aligned to the ith element in x if i>=0 
//the jth element in y is aligned to a gap in x if i==-1
double get_initial( double **x, 
                    double **y, 
                    int x_len,
                    int y_len, 
                    int *y2x
                   )
{
    int min_len=getmin(x_len, y_len);
    if(min_len<=5) PrintErrorAndQuit("Sequence is too short <=5!\n");
    
    int min_ali= min_len/2;              //minimum size of considered fragment 
    if(min_ali<=5)  min_ali=5;    
    int n1, n2;
    n1 = -y_len+min_ali; 
    n2 = x_len-min_ali;

    int i, j, k, k_best;
    double tmscore, tmscore_max=-1;

    k_best=n1;
    for(k=n1; k<=n2; k+=(fast_opt)?5:1)
    {
        //get the map
        for(j=0; j<y_len; j++)
        {
            i=j+k;
            if(i>=0 && i<x_len)
            {
                y2x[j]=i;
            }
            else
            {
                y2x[j]=-1;
            }
        }
        
        //evaluate the map quickly in three iterations
        //this is not real tmscore, it is used to evaluate the goodness of the initial alignment
        tmscore=get_score_fast(x, y, x_len, y_len, y2x); 
        if(tmscore>=tmscore_max)
        {
            tmscore_max=tmscore;
            k_best=k;
        }
    }
    
    //extract the best map
    k=k_best;
    for(j=0; j<y_len; j++)
    {
        i=j+k;
        if(i>=0 && i<x_len)
        {
            y2x[j]=i;
        }
        else
        {
            y2x[j]=-1;
        }
    }    

    return tmscore_max;
}

void smooth(int *sec, int len)
{
    int i, j;
    //smooth single  --x-- => -----
    for(i=2; i<len-2; i++)
    {
        if(sec[i]==2 || sec[i]==4)
        {
            j=sec[i];
            if(sec[i-2] != j)
            {
                if(sec[i-1] != j)
                {
                    if(sec[i+1] != j)
                    {
                        if(sec[i+2] != j) 
                        {
                            sec[i]=1;
                        }
                    }
                }
            }
        }
    }

    //   smooth double 
    //   --xx-- => ------

    for(i=0; i<len-5; i++)
    {
        //helix
        if(sec[i] != 2)
        {
            if(sec[i+1] != 2)
            {
                if(sec[i+2] == 2)
                {
                    if(sec[i+3] == 2)
                    {
                        if(sec[i+4] != 2)
                        {
                            if(sec[i+5] != 2)
                            {
                                sec[i+2]=1;
                                sec[i+3]=1;
                            }
                        }
                    }
                }
            }
        }

        //beta
        if(sec[i] != 4)
        {
            if(sec[i+1] != 4)
            {
                if(sec[i+2] ==4)
                {
                    if(sec[i+3] == 4)
                    {
                        if(sec[i+4] != 4)
                        {
                            if(sec[i+5] != 4)
                            {
                                sec[i+2]=1;
                                sec[i+3]=1;
                            }
                        }
                    }
                }
            }
        }
    }

    //smooth connect
    for(i=0; i<len-2; i++)
    {        
        if(sec[i] == 2)
        {
            if(sec[i+1] != 2)
            {
                if(sec[i+2] == 2)
                {
                    sec[i+1]=2;
                }
            }
        }
        else if(sec[i] == 4)
        {
            if(sec[i+1] != 4)
            {
                if(sec[i+2] == 4)
                {
                    sec[i+1]=4;
                }
            }
        }
    }

}

int sec_str(double dis13, double dis14, double dis15, double dis24, double dis25, double dis35)
{
    int s=1;
    
    double delta=2.1;
    if(fabs(dis15-6.37)<delta)
    {
        if(fabs(dis14-5.18)<delta)
        {
            if(fabs(dis25-5.18)<delta)
            {
                if(fabs(dis13-5.45)<delta)
                {
                    if(fabs(dis24-5.45)<delta)
                    {
                        if(fabs(dis35-5.45)<delta)
                        {
                            s=2; //helix                        
                            return s;
                        }
                    }
                }
            }
        }
    }

    delta=1.42;
    if(fabs(dis15-13)<delta)
    {
        if(fabs(dis14-10.4)<delta)
        {
            if(fabs(dis25-10.4)<delta)
            {
                if(fabs(dis13-6.1)<delta)
                {
                    if(fabs(dis24-6.1)<delta)
                    {
                        if(fabs(dis35-6.1)<delta)
                        {
                            s=4; //strand
                            return s;
                        }
                    }
                }
            }
        }
    }

    if(dis15 < 8)
    {
        s=3; //turn
    }      


    return s;
}


//1->coil, 2->helix, 3->turn, 4->strand
void make_sec(double **x, int len, int *sec)
{
    int j1, j2, j3, j4, j5;
    double d13, d14, d15, d24, d25, d35;
    for(int i=0; i<len; i++)
    {     
        sec[i]=1;
        j1=i-2;
        j2=i-1;
        j3=i;
        j4=i+1;
        j5=i+2;        
        
        if(j1>=0 && j5<len)
        {
            d13=sqrt(dist(x[j1], x[j3]));
            d14=sqrt(dist(x[j1], x[j4]));
            d15=sqrt(dist(x[j1], x[j5]));
            d24=sqrt(dist(x[j2], x[j4]));
            d25=sqrt(dist(x[j2], x[j5]));
            d35=sqrt(dist(x[j3], x[j5]));
            sec[i]=sec_str(d13, d14, d15, d24, d25, d35);            
        }    
    } 
}




//get initial alignment from secondary structure alignment
//input: x, y, x_len, y_len
//output: y2x stores the best alignment: e.g., 
//y2x[j]=i means:
//the jth element in y is aligned to the ith element in x if i>=0 
//the jth element in y is aligned to a gap in x if i==-1
void get_initial_ss(  double **x, 
                      double **y, 
                      int x_len,
                      int y_len, 
                      int *y2x
                      )
{
    //assign secondary structures
    make_sec(x, x_len, secx);
    make_sec(y, y_len, secy);

    double gap_open=-1.0;
    NWDP_TM(secx, secy, x_len, y_len, gap_open, y2x);    
}


// get_initial5 in TMalign fortran, get_intial_local in TMalign c by yangji
//get initial alignment of local structure superposition
//input: x, y, x_len, y_len
//output: y2x stores the best alignment: e.g., 
//y2x[j]=i means:
//the jth element in y is aligned to the ith element in x if i>=0 
//the jth element in y is aligned to a gap in x if i==-1
bool get_initial5(double **x,
    double **y,
    int x_len,
    int y_len,
    int *y2x
    )
{
    double GL, rmsd;
    double t[3];
    double u[3][3];

    double d01 = d0 + 1.5;
    if (d01 < D0_MIN) d01 = D0_MIN;
    double d02 = d01*d01;

    double GLmax = 0;
    int aL = getmin(x_len, y_len);
    int *invmap = new int[y_len + 1];

    // jump on sequence1-------------->
    int n_jump1 = 0;
    if (x_len > 250)
        n_jump1 = 45;
    else if (x_len > 200)
        n_jump1 = 35;
    else if (x_len > 150)
        n_jump1 = 25;
    else
        n_jump1 = 15;
    if (n_jump1 > (x_len / 3))
        n_jump1 = x_len / 3;

    // jump on sequence2-------------->
    int n_jump2 = 0;
    if (y_len > 250)
        n_jump2 = 45;
    else if (y_len > 200)
        n_jump2 = 35;
    else if (y_len > 150)
        n_jump2 = 25;
    else
        n_jump2 = 15;
    if (n_jump2 > (y_len / 3))
        n_jump2 = y_len / 3;

    // fragment to superimpose-------------->
    int n_frag[2] = { 20, 100 };
    if (n_frag[0] > (aL / 3))
        n_frag[0] = aL / 3;
    if (n_frag[1] > (aL / 2))
        n_frag[1] = aL / 2;

    // start superimpose search-------------->
    if (fast_opt)
    {
        n_jump1*=5;
        n_jump2*=5;
    }
    bool flag = false;
    for (int i_frag = 0; i_frag < 2; i_frag++)
    {
        int m1 = x_len - n_frag[i_frag] + 1;
        int m2 = y_len - n_frag[i_frag] + 1;

        for (int i = 0; i<m1; i = i + n_jump1) //index starts from 0, different from FORTRAN
        {
            for (int j = 0; j<m2; j = j + n_jump2)
            {
                for (int k = 0; k<n_frag[i_frag]; k++) //fragment in y
                {
                    r1[k][0] = x[k + i][0];
                    r1[k][1] = x[k + i][1];
                    r1[k][2] = x[k + i][2];

                    r2[k][0] = y[k + j][0];
                    r2[k][1] = y[k + j][1];
                    r2[k][2] = y[k + j][2];
                }

                // superpose the two structures and rotate it
                Kabsch(r1, r2, n_frag[i_frag], 1, &rmsd, t, u);

                double gap_open = 0.0;
                NWDP_TM(x, y, x_len, y_len, t, u, d02, gap_open, invmap);
                GL = get_score_fast(x, y, x_len, y_len, invmap);
                if (GL>GLmax)
                {
                    GLmax = GL;
                    for (int ii = 0; ii<y_len; ii++)
                    {
                        y2x[ii] = invmap[ii];
                    }
                    flag = true;
                }
            }
        }
    }

    delete[] invmap;
    return flag;
}

//with invmap(i) calculate score(i,j) using RMSD rotation
void score_matrix_rmsd(  double **x, 
                         double **y, 
                         int x_len,
                         int y_len,
                         int *y2x
                         )
{
    double t[3], u[3][3];
    double rmsd, dij;
    double d01=d0+1.5;
    if(d01 < D0_MIN) d01=D0_MIN;
    double d02=d01*d01;

    double xx[3];
    int i, k=0;
    for(int j=0; j<y_len; j++)
    {
        i=y2x[j];
        if(i>=0)
        {
            r1[k][0]=x[i][0];  
            r1[k][1]=x[i][1]; 
            r1[k][2]=x[i][2];   
            
            r2[k][0]=y[j][0];  
            r2[k][1]=y[j][1]; 
            r2[k][2]=y[j][2];
            
            k++;
        }
    }
    Kabsch(r1, r2, k, 1, &rmsd, t, u);
    //do_rotation(x, xt, x_len, t, u);
    
    
    for(int ii=0; ii<x_len; ii++)
    {        
        transform(t, u, &x[ii][0], xx);
        for(int jj=0; jj<y_len; jj++)
        {
            //dij=dist(&xt[ii][0], &y[jj][0]);   
            dij=dist(xx, &y[jj][0]); 
            score[ii+1][jj+1] = 1.0/(1+dij/d02);
            //    cout << ii+1 << " " << jj+1 << " " << score[ii+1][jj+1]<< endl;
        }
    }        
}


void score_matrix_rmsd_sec(  double **x, 
                             double **y, 
                             int x_len,
                             int y_len,
                             int *y2x
                             )
{
    double t[3], u[3][3];
    double rmsd, dij;
    double d01=d0+1.5;
    if(d01 < D0_MIN) d01=D0_MIN;
    double d02=d01*d01;

    double xx[3];
    int i, k=0;
    for(int j=0; j<y_len; j++)
    {
        i=y2x[j];
        if(i>=0)
        {
            r1[k][0]=x[i][0];  
            r1[k][1]=x[i][1]; 
            r1[k][2]=x[i][2];   
            
            r2[k][0]=y[j][0];  
            r2[k][1]=y[j][1]; 
            r2[k][2]=y[j][2];
            
            k++;
        }
    }
    Kabsch(r1, r2, k, 1, &rmsd, t, u);

    
    for(int ii=0; ii<x_len; ii++)
    {        
        transform(t, u, &x[ii][0], xx);
        for(int jj=0; jj<y_len; jj++)
        {
            dij=dist(xx, &y[jj][0]); 
            if(secx[ii]==secy[jj])
            {
                score[ii+1][jj+1] = 1.0/(1+dij/d02) + 0.5;
            }
            else
            {
                score[ii+1][jj+1] = 1.0/(1+dij/d02);
            }        
        }
    }        
}


//get initial alignment from secondary structure and previous alignments
//input: x, y, x_len, y_len
//output: y2x stores the best alignment: e.g., 
//y2x[j]=i means:
//the jth element in y is aligned to the ith element in x if i>=0 
//the jth element in y is aligned to a gap in x if i==-1
void get_initial_ssplus( double **x, 
                         double **y, 
                         int x_len,
                         int y_len,
                         int *y2x0,
                         int *y2x                        
                         )
{

    //create score matrix for DP
    score_matrix_rmsd_sec(x, y, x_len, y_len, y2x0);
    
    double gap_open=-1.0;
    NWDP_TM(x_len, y_len, gap_open, y2x);
}


void find_max_frag(double **x, int *resno, int len, int *start_max, int *end_max)
{
    int r_min, fra_min=4;           //minimum fragment for search
    if (fast_opt) fra_min=8;
    double d;
    int start;
    int Lfr_max=0, flag;

    r_min= (int) (len*1.0/3.0); //minimum fragment, in case too small protein
    if(r_min > fra_min) r_min=fra_min;
    
    int inc=0;
    double dcu0_cut=dcu0*dcu0;;
    double dcu_cut=dcu0_cut;

    while(Lfr_max < r_min)
    {        
        Lfr_max=0;            
        int j=1;    //number of residues at nf-fragment
        start=0;
        for(int i=1; i<len; i++)
        {            
            d = dist(x[i-1], x[i]);
            flag=0;
            if(dcu_cut>dcu0_cut)
            {
                if(d<dcu_cut)
                {
                    flag=1;
                }
            }
            else if(resno[i] == (resno[i-1]+1)) //necessary??
            {
                if(d<dcu_cut)
                {
                    flag=1;
                }
            }

            if(flag==1)
            {
                j++;

                if(i==(len-1))
                {
                    if(j > Lfr_max) 
                    {
                        Lfr_max=j;
                        *start_max=start;
                        *end_max=i;                        
                    }
                    j=1;
                }
            }
            else
            {
                if(j>Lfr_max) 
                {
                    Lfr_max=j;
                    *start_max=start;
                    *end_max=i-1;                                        
                }

                j=1;
                start=i;
            }
        }// for i;
        
        if(Lfr_max < r_min)
        {
            inc++;
            double dinc=pow(1.1, (double) inc) * dcu0;
            dcu_cut= dinc*dinc;
        }
    }//while <;    
}

//perform fragment gapless threading to find the best initial alignment
//input: x, y, x_len, y_len
//output: y2x0 stores the best alignment: e.g., 
//y2x0[j]=i means:
//the jth element in y is aligned to the ith element in x if i>=0 
//the jth element in y is aligned to a gap in x if i==-1
double get_initial_fgt( double **x, 
                        double **y, 
                        int x_len,
                        int y_len, 
                        int *xresno,
                        int *yresno,
                        int *y2x
                        )
{
    int fra_min=4;           //minimum fragment for search
    if (fast_opt) fra_min=8;
    int fra_min1=fra_min-1;  //cutoff for shift, save time

    int xstart=0, ystart=0, xend=0, yend=0;

    find_max_frag(x, xresno, x_len,  &xstart, &xend);
    find_max_frag(y, yresno, y_len, &ystart, &yend);


    int Lx = xend-xstart+1;
    int Ly = yend-ystart+1;
    int *ifr, *y2x_;
    int L_fr=getmin(Lx, Ly);
    ifr= new int[L_fr];
    y2x_= new int[y_len+1];

    //select what piece will be used (this may araise ansysmetry, but
    //only when L1=L2 and Lfr1=Lfr2 and L1 ne Lfr1
    //if L1=Lfr1 and L2=Lfr2 (normal proteins), it will be the same as initial1

    if(Lx<Ly || (Lx==Ly && x_len<=y_len))
    {        
        for(int i=0; i<L_fr; i++)
        {
            ifr[i]=xstart+i;
        }
    }
    else if(Lx>Ly || (Lx==Ly && x_len>y_len))
    {        
        for(int i=0; i<L_fr; i++)
        {
            ifr[i]=ystart+i;
        }    
    }

    
    int L0=getmin(x_len, y_len); //non-redundant to get_initial1
    if(L_fr==L0)
    {
        int n1= (int)(L0*0.1); //my index starts from 0
        int n2= (int)(L0*0.89);

        int j=0;
        for(int i=n1; i<= n2; i++)
        {
            ifr[j]=ifr[i];
            j++;
        }
        L_fr=j;
    }


    //gapless threading for the extracted fragment
    double tmscore, tmscore_max=-1;

    if(Lx<Ly || (Lx==Ly && x_len<=y_len))
    {
        int L1=L_fr;
        int min_len=getmin(L1, y_len);    
        int min_ali= (int) (min_len/2.5);              //minimum size of considered fragment 
        if(min_ali<=fra_min1)  min_ali=fra_min1;    
        int n1, n2;
        n1 = -y_len+min_ali; 
        n2 = L1-min_ali;

        int i, j, k;
        for(k=n1; k<=n2; k+=(fast_opt)?3:1)
        {
            //get the map
            for(j=0; j<y_len; j++)
            {
                i=j+k;
                if(i>=0 && i<L1)
                {                
                    y2x_[j]=ifr[i];
                }
                else
                {
                    y2x_[j]=-1;
                }
            }

            //evaluate the map quickly in three iterations
            tmscore=get_score_fast(x, y, x_len, y_len, y2x_);

            if(tmscore>=tmscore_max)
            {
                tmscore_max=tmscore;
                for(j=0; j<y_len; j++)
                {
                    y2x[j]=y2x_[j];
                }
            }
        }
    }
    else
    {
        int L2=L_fr;
        int min_len=getmin(x_len, L2);    
        int min_ali= (int) (min_len/2.5);              //minimum size of considered fragment 
        if(min_ali<=fra_min1)  min_ali=fra_min1;    
        int n1, n2;
        n1 = -L2+min_ali; 
        n2 = x_len-min_ali;

        int i, j, k;    

        for(k=n1; k<=n2; k++)
        {
            //get the map
            for(j=0; j<y_len; j++)
            {
                y2x_[j]=-1;
            }

            for(j=0; j<L2; j++)
            {
                i=j+k;
                if(i>=0 && i<x_len)
                {
                    y2x_[ifr[j]]=i;
                }
            }
        
            //evaluate the map quickly in three iterations
            tmscore=get_score_fast(x, y, x_len, y_len, y2x_);
            if(tmscore>=tmscore_max)
            {
                tmscore_max=tmscore;
                for(j=0; j<y_len; j++)
                {
                    y2x[j]=y2x_[j];
                }
            }
        }
    }    


    delete [] ifr;
    delete [] y2x_;
    return tmscore_max;
}





//heuristic run of dynamic programing iteratively to find the best alignment
//input: initial rotation matrix t, u
//       vectors x and y, d0
//output: best alignment that maximizes the TMscore, will be stored in invmap
double DP_iter( double **x,
                double **y, 
                int x_len, 
                int y_len, 
                double t[3],
                double u[3][3],
                int invmap0[],
                int g1,
                int g2,
                int iteration_max,
                double local_d0_search
                )
{
    double gap_open[2]={-0.6, 0};
    double rmsd; 
    int *invmap=new int[y_len+1];
    
    int iteration, i, j, k;
    double tmscore, tmscore_max, tmscore_old=0;    
    int score_sum_method=8, simplify_step=40;
    tmscore_max=-1;

    //double d01=d0+1.5;
    double d02=d0*d0;
    for(int g=g1; g<g2; g++)
    {
        for(iteration=0; iteration<iteration_max; iteration++)
        {           
            NWDP_TM(x, y, x_len, y_len, t, u, d02, gap_open[g], invmap);
            
            k=0;
            for(j=0; j<y_len; j++) 
            {
                i=invmap[j];

                if(i>=0) //aligned
                {
                    xtm[k][0]=x[i][0];
                    xtm[k][1]=x[i][1];
                    xtm[k][2]=x[i][2];
                    
                    ytm[k][0]=y[j][0];
                    ytm[k][1]=y[j][1];
                    ytm[k][2]=y[j][2];
                    k++;
                }
            }

            //tmscore=TMscore8_search(xtm, ytm, k, t, u, simplify_step, score_sum_method, &rmsd);
            tmscore = TMscore8_search(xtm, ytm, k, t, u, simplify_step, score_sum_method, &rmsd, local_d0_search);

           
            if(tmscore>tmscore_max)
            {
                tmscore_max=tmscore;
                for(i=0; i<y_len; i++) 
                {                
                    invmap0[i]=invmap[i];                                      
                }                
            }
    
            if(iteration>0)
            {
                if(fabs(tmscore_old-tmscore)<0.000001)
                {     
                    break;       
                }
            }
            tmscore_old=tmscore;
        }// for iteration           
        
    }//for gapopen
    
    
    delete []invmap;
    return tmscore_max;
}


void output_superpose(const char *xname, double t[3], double u[3][3], 
    const int ter_opt=3)
{
    ifstream fin(xname);
    stringstream buf;
    string line;
    double x[3];  // before transform
    double x1[3]; // after transform
    if (fin.is_open())
    {
        while (fin.good())
        {
            getline(fin, line);
            if (line.compare(0, 6, "ATOM  ")==0 || 
                line.compare(0, 6, "HETATM")==0)
            {
                x[0]=atof(line.substr(30,8).c_str());
                x[1]=atof(line.substr(38,8).c_str());
                x[2]=atof(line.substr(46,8).c_str());
                transform(t, u, x, x1);
                buf<<line.substr(0,30)<<setiosflags(ios::fixed)
                    <<setprecision(3)
                    <<setw(8)<<x1[0] <<setw(8)<<x1[1] <<setw(8)<<x1[2]
                    <<line.substr(54)<<'\n';
            }
            else if (line.size())
                buf<<line<<'\n';
            if (ter_opt>=1 && line.compare(0,3,"END")==0) break;
        }
        fin.close();
    }
    else
    {
        char message[5000];
        sprintf(message, "Can not open file: %s\n", xname);
        PrintErrorAndQuit(message);
    }

    ofstream fp(out_reg);
    fp<<buf.str();
    fp.close();
    buf.str(string()); // clear stream
}

/* extract rotation matrix based on TMscore8 */
void output_rotation_matrix(const char* fname_matrix,
    const double t[3], const double u[3][3])
{
    fstream fout;
    fout.open(fname_matrix, ios::out | ios::trunc);
    if (fout)// succeed
    {
        fout << "------ The rotation matrix to rotate Chain_1 to Chain_2 ------\n";
        char dest[1000];
        sprintf(dest, "m %18s %14s %14s %14s\n", "t[m]", "u[m][0]", "u[m][1]", "u[m][2]");
        fout << string(dest);
        for (int k = 0; k < 3; k++)
        {
            sprintf(dest, "%d %18.10f %14.10f %14.10f %14.10f\n", k, t[k], u[k][0], u[k][1], u[k][2]);
            fout << string(dest);
        }
        fout << "\nCode for rotating Structure A from (x,y,z) to (X,Y,Z):\n"
                "for(i=0; i<L; i++)\n"
                "{\n"
                "   X[i] = t[0] + u[0][0]*x[i] + u[0][1]*y[i] + u[0][2]*z[i]\n"
                "   Y[i] = t[1] + u[1][0]*x[i] + u[1][1]*y[i] + u[1][2]*z[i]\n"
                "   Z[i] = t[2] + u[2][0]*x[i] + u[2][1]*y[i] + u[2][2]*z[i]\n"
                "}\n";
        fout.close();
    }
    else
        cout << "Open file to output rotation matrix fail.\n";
}

//output the final results
void output_results(
    const char *xname,
    const char *yname,
    int x_len,
    int y_len,
    double t[3],
    double u[3][3],
    double TM1,
    double TM2,
    double rmsd,
    double d0_out,
    int m1[],
    int m2[],
    int n_ali8,
    int n_ali,
    double TM_0,
    double Lnorm_0,
    double d0_0,
    const char* fname_matrix,
    const string dir1_opt,
    const string dir2_opt,
    const int outfmt_opt,    
    const int ter_opt)
{
    double seq_id;          
    int i, j, k;
    double d;
    int ali_len=x_len+y_len; //maximum length of alignment
    char *seqM, *seqxA, *seqyA;
    seqM=new char[ali_len];
    seqxA=new char[ali_len];
    seqyA=new char[ali_len];
    
    if (outfmt_opt<=0) do_rotation(xa, xt, x_len, t, u);

    seq_id=0;
    int kk=0, i_old=0, j_old=0;
    for(k=0; k<n_ali8; k++)
    {
        for(i=i_old; i<m1[k]; i++)
        {
            //align x to gap
            seqxA[kk]=seqx[i];
            seqyA[kk]='-';
            seqM[kk]=' ';                    
            kk++;
        }

        for(j=j_old; j<m2[k]; j++)
        {
            //align y to gap
            seqxA[kk]='-';
            seqyA[kk]=seqy[j];
            seqM[kk]=' ';
            kk++;
        }

        seqxA[kk]=seqx[m1[k]];
        seqyA[kk]=seqy[m2[k]];
        if(seqxA[kk]==seqyA[kk])
        {
            seq_id++;
        }
        if (outfmt_opt<=0)
        {
            d=sqrt(dist(&xt[m1[k]][0], &ya[m2[k]][0]));
            if(d<d0_out) seqM[kk]=':';
            else         seqM[kk]='.';
        } 
        kk++;  
        i_old=m1[k]+1;
        j_old=m2[k]+1;
    }

    //tail
    for(i=i_old; i<x_len; i++)
    {
        //align x to gap
        seqxA[kk]=seqx[i];
        seqyA[kk]='-';
        seqM[kk]=' ';                    
        kk++;
    }    
    for(j=j_old; j<y_len; j++)
    {
        //align y to gap
        seqxA[kk]='-';
        seqyA[kk]=seqy[j];
        seqM[kk]=' ';
        kk++;
    }
 
    seqxA[kk]='\0';
    seqyA[kk]='\0';
    seqM[kk]='\0';
    
    if (outfmt_opt<=0)
    {
        printf("\nName of Chain_1: %s (to be superimposed onto Chain_2)\n",
            xname+dir1_opt.size());
        printf("Name of Chain_2: %s\n", yname+dir2_opt.size());
        printf("Length of Chain_1: %d residues\n", x_len);
        printf("Length of Chain_2: %d residues\n\n", y_len);

        if (i_opt || I_opt)
            printf("User-specified initial alignment: TM/Lali/rmsd = %7.5lf, %4d, %6.3lf\n", TM_ali, L_ali, rmsd_ali);

        printf("Aligned length= %d, RMSD= %6.2f, Seq_ID=n_identical/n_aligned= %4.3f\n", n_ali8, rmsd, seq_id/( n_ali8+0.00000001));
        printf("TM-score= %6.5f (if normalized by length of Chain_1, i.e., LN=%d, d0=%.2f)\n", TM2, x_len, d0B);
        printf("TM-score= %6.5f (if normalized by length of Chain_2, i.e., LN=%d, d0=%.2f)\n", TM1, y_len, d0A);

        if(a_opt)
            printf("TM-score= %6.5f (if normalized by average length of two structures, i.e., LN= %.2f, d0= %.2f)\n", TM3, (x_len+y_len)*0.5, d0a);
        if(u_opt)
            printf("TM-score= %6.5f (if normalized by user-specified LN=%.2f and d0=%.2f)\n", TM4, Lnorm_ass, d0u);
        if(d_opt)
            printf("TM-score= %6.5f (if scaled by user-specified d0= %.2f, and LN= %.2f)\n", TM5, d0_scale, Lnorm_0);
        printf("(You should use TM-score normalized by length of the reference protein)\n");
    
        //output alignment
        printf("\n(\":\" denotes residue pairs of d < %4.1f Angstrom, ", d0_out);
        printf("\".\" denotes other aligned residues)\n");
        printf("%s\n", seqxA);
        printf("%s\n", seqM);
        printf("%s\n", seqyA);
    }
    else if (outfmt_opt==1)
    {
        printf(">%s\tL=%d\td0=%.2f\tseqID=%.3f\tTM-score=%.5f\n",
            xname+dir1_opt.size(), x_len, d0B, seq_id/x_len, TM2);
        printf("%s\n", seqxA);
        printf(">%s\tL=%d\td0=%.2f\tseqID=%.3f\tTM-score=%.5f\n",
            yname+dir2_opt.size(), y_len, d0A, seq_id/y_len, TM1);
        printf("%s\n", seqyA);

        printf("# Lali=%d\tRMSD=%.2f\tseqID_ali=%.3f\n",
            n_ali8, rmsd, seq_id/(n_ali8+0.00000001));

        if (i_opt || I_opt)
            printf("# User-specified initial alignment: TM=%.5lf\tLali=%4d\trmsd=%.3lf\n", TM_ali, L_ali, rmsd_ali);

        if(a_opt)
            printf("# TM-score=%.5f (normalized by average length of two structures: L=%.2f\td0=%.2f)\n", TM3, (x_len+y_len)*0.5, d0a);

        if(u_opt)
            printf("# TM-score=%.5f (normalized by user-specified L=%.2f\td0=%.2f)\n", TM4, Lnorm_ass, d0u);

        if(d_opt)
            printf("# TM-score=%.5f (scaled by user-specified d0=%.2f\tL=%.2f)\n", TM5, d0_scale, Lnorm_0);

        printf("$$$$\n");
    }
    else if (outfmt_opt==2)
    {
        printf("%s\t%s\t%.4f\t%.4f\t%.2f\t%.3f\t%4.3f\t%4.3f\t%d\t%d\t%d",
            xname+dir1_opt.size(), yname+dir2_opt.size(),
            TM2, TM1, rmsd,
            seq_id/x_len, seq_id/y_len, seq_id/( n_ali8+0.00000001),
            x_len, y_len, n_ali8);
    }
    cout << endl;

    if (m_opt) output_rotation_matrix(fname_matrix, t, u);
    if (o_opt) output_superpose(xname, t, u, ter_opt);

    delete [] seqM;
    delete [] seqxA;
    delete [] seqyA;
}

double standard_TMscore(double **x, double **y, int x_len, int y_len, int invmap[], int& L_ali, double& RMSD )
{
    D0_MIN = 0.5;
    Lnorm = y_len;
    if (Lnorm > 21)
        d0 = (1.24*pow((Lnorm*1.0 - 15), 1.0 / 3) - 1.8);
    else
        d0 = D0_MIN;
    if (d0 < D0_MIN)
        d0 = D0_MIN;
    double d0_input = d0;// Scaled by seq_min

    double tmscore;// collected alined residues from invmap
    int n_al = 0;
    int i;
    for (int j = 0; j<y_len; j++)
    {
        i = invmap[j];
        if (i >= 0)
        {
            xtm[n_al][0] = x[i][0];
            xtm[n_al][1] = x[i][1];
            xtm[n_al][2] = x[i][2];

            ytm[n_al][0] = y[j][0];
            ytm[n_al][1] = y[j][1];
            ytm[n_al][2] = y[j][2];

            r1[n_al][0] = x[i][0];
            r1[n_al][1] = x[i][1];
            r1[n_al][2] = x[i][2];

            r2[n_al][0] = y[j][0];
            r2[n_al][1] = y[j][1];
            r2[n_al][2] = y[j][2];

            n_al++;
        }
        else if (i != -1)
        {
            PrintErrorAndQuit("Wrong map!\n");
        }
    }
    L_ali = n_al;

    Kabsch(r1, r2, n_al, 0, &RMSD, t, u);
    RMSD = sqrt( RMSD/(1.0*n_al) );
    
    int temp_simplify_step = 1;
    int temp_score_sum_method = 0;
    d0_search = d0_input;
    double rms = 0.0;
    tmscore = TMscore8_search_standard(xtm, ytm, n_al, t, u, temp_simplify_step, temp_score_sum_method, &rms, d0_input);
    tmscore = tmscore * n_al / (1.0*Lnorm);

    return tmscore;
}

/* entry function for TMalign */
int TMalign_main(const char *xname, const char *yname,
    const char *fname_matrix, const int ter_opt,
    const string dir1_opt, const string dir2_opt, const int outfmt_opt)
{
    /***********************/
    /*    parameter set    */
    /***********************/
    parameter_set4search(xlen, ylen);          //please set parameters in the function
    int simplify_step     = 40;               //for similified search engine
    int score_sum_method  = 8;                //for scoring method, whether only sum over pairs with dis<score_d8

    int i;
    int *invmap0          = new int[ylen+1];
    int *invmap           = new int[ylen+1];
    double TM, TMmax=-1;
    for(i=0; i<ylen; i++)
    {
        invmap0[i]=-1;
    }


    double ddcc=0.4;
    if(Lnorm <= 40) ddcc=0.1;   //Lnorm was setted in parameter_set4search
    double local_d0_search = d0_search;

    //************************************************//
    //    get initial alignment from user's input:    //
    //    Stick to the initial alignment              //
    //************************************************//
    char dest[1000];
    bool bAlignStick = false;
    if (I_opt)// if input has set parameter for "-I"
    {
        for (int j = 1; j < ylen; j++)// Set aligned position to be "-1"
            invmap[j] = -1;

        int i1 = -1;// in C version, index starts from zero, not from one
        int i2 = -1;
        int L1 = strlen(sequence[0]);
        int L2 = strlen(sequence[1]);
        int L = min(L1, L2);// Get positions for aligned residues
        for (int kk1 = 0; kk1 < L; kk1++)
        {
            if (sequence[0][kk1] != '-')
                i1++;
            if (sequence[1][kk1] != '-')
            {
                i2++;
                if (i2 >= ylen || i1 >= xlen)
                    kk1 = L;
                else
                {
                    if (sequence[0][kk1] != '-')
                    {
                        invmap[i2] = i1;
                    }
                }
            }
        }

        //--------------- 2. Align proteins from original alignment
        double prevD0_MIN = D0_MIN;// stored for later use
        int prevLnorm = Lnorm;
        double prevd0 = d0;
        TM_ali = standard_TMscore(xa, ya, xlen, ylen, invmap, L_ali, rmsd_ali);
        D0_MIN = prevD0_MIN;
        Lnorm = prevLnorm;
        d0 = prevd0;
        TM = detailed_search_standard(xa, ya, xlen, ylen, invmap, t, u, 40, 8, local_d0_search, true);
        if (TM > TMmax)
        {
            TMmax = TM;
            for (i = 0; i<ylen; i++)
                invmap0[i] = invmap[i];
        }
        bAlignStick = true;
    }

    /******************************************************/
    /*    get initial alignment with gapless threading    */
    /******************************************************/
    if (!bAlignStick)
    {
        get_initial(xa, ya, xlen, ylen, invmap0);
        //find the max TMscore for this initial alignment with the simplified search_engin
        TM = detailed_search(xa, ya, xlen, ylen, invmap0, t, u, simplify_step, score_sum_method, local_d0_search);
        if (TM>TMmax)
        {
            TMmax = TM;
        }
        //run dynamic programing iteratively to find the best alignment
        TM = DP_iter(xa, ya, xlen, ylen, t, u, invmap, 0, 2, (fast_opt)?2:30, local_d0_search);
        if (TM>TMmax)
        {
            TMmax = TM;
            for (int i = 0; i<ylen; i++)
            {
                invmap0[i] = invmap[i];
            }
        }


        /************************************************************/
        /*    get initial alignment based on secondary structure    */
        /************************************************************/
        get_initial_ss(xa, ya, xlen, ylen, invmap);
        TM = detailed_search(xa, ya, xlen, ylen, invmap, t, u, simplify_step, score_sum_method, local_d0_search);
        if (TM>TMmax)
        {
            TMmax = TM;
            for (int i = 0; i<ylen; i++)
            {
                invmap0[i] = invmap[i];
            }
        }
        if (TM > TMmax*0.2)
        {
            TM = DP_iter(xa, ya, xlen, ylen, t, u, invmap, 0, 2, (fast_opt)?2:30, local_d0_search);
            if (TM>TMmax)
            {
                TMmax = TM;
                for (int i = 0; i<ylen; i++)
                {
                    invmap0[i] = invmap[i];
                }
            }
        }


        /************************************************************/
        /*    get initial alignment based on local superposition    */
        /************************************************************/
        //=initial5 in original TM-align
        if (get_initial5(xa, ya, xlen, ylen, invmap))
        {
            TM = detailed_search(xa, ya, xlen, ylen, invmap, t, u, simplify_step, score_sum_method, local_d0_search);
            if (TM>TMmax)
            {
                TMmax = TM;
                for (int i = 0; i<ylen; i++)
                {
                    invmap0[i] = invmap[i];
                }
            }
            if (TM > TMmax*ddcc)
            {
                TM = DP_iter(xa, ya, xlen, ylen, t, u, invmap, 0, 2, 2, local_d0_search);
                if (TM>TMmax)
                {
                    TMmax = TM;
                    for (int i = 0; i<ylen; i++)
                    {
                        invmap0[i] = invmap[i];
                    }
                }
            }
        }
        else
        {
            cout << "\n\nWarning: initial alignment from local superposition fail!\n\n" << endl;
        }


        /*******************************************************************************/
        /*    get initial alignment based on previous alignment+secondary structure    */
        /*******************************************************************************/
        //=initial3 in original TM-align
        get_initial_ssplus(xa, ya, xlen, ylen, invmap0, invmap);
        TM = detailed_search(xa, ya, xlen, ylen, invmap, t, u, simplify_step, score_sum_method, local_d0_search);
        if (TM>TMmax)
        {
            TMmax = TM;
            for (i = 0; i<ylen; i++)
            {
                invmap0[i] = invmap[i];
            }
        }
        if (TM > TMmax*ddcc)
        {
            TM = DP_iter(xa, ya, xlen, ylen, t, u, invmap, 0, 2, (fast_opt)?2:30, local_d0_search);
            if (TM>TMmax)
            {
                TMmax = TM;
                for (i = 0; i<ylen; i++)
                {
                    invmap0[i] = invmap[i];
                }
            }
        }


        /*******************************************************************/
        /*    get initial alignment based on fragment gapless threading    */
        /*******************************************************************/
        //=initial4 in original TM-align
        get_initial_fgt(xa, ya, xlen, ylen, xresno, yresno, invmap);
        TM = detailed_search(xa, ya, xlen, ylen, invmap, t, u, simplify_step, score_sum_method, local_d0_search);
        if (TM>TMmax)
        {
            TMmax = TM;
            for (i = 0; i<ylen; i++)
            {
                invmap0[i] = invmap[i];
            }
        }
        if (TM > TMmax*ddcc)
        {
            TM = DP_iter(xa, ya, xlen, ylen, t, u, invmap, 1, 2, 2, local_d0_search);
            if (TM>TMmax)
            {
                TMmax = TM;
                for (i = 0; i<ylen; i++)
                {
                    invmap0[i] = invmap[i];
                }
            }
        }


        //************************************************//
        //    get initial alignment from user's input:    //
        //************************************************//
        if (i_opt)// if input has set parameter for "-i"
        {
            for (int j = 0; j < ylen; j++)// Set aligned position to be "-1"
                invmap[j] = -1;

            int i1 = -1;// in C version, index starts from zero, not from one
            int i2 = -1;
            int L1 = strlen(sequence[0]);
            int L2 = strlen(sequence[1]);
            int L = min(L1, L2);// Get positions for aligned residues
            for (int kk1 = 0; kk1 < L; kk1++)
            {
                if (sequence[0][kk1] != '-')
                    i1++;
                if (sequence[1][kk1] != '-')
                {
                    i2++;
                    if (i2 >= ylen || i1 >= xlen)
                        kk1 = L;
                    else
                    {
                        if (sequence[0][kk1] != '-')
                        {
                            invmap[i2] = i1;
                        }
                    }
                }
            }

            //--------------- 2. Align proteins from original alignment
            double prevD0_MIN = D0_MIN;// stored for later use
            int prevLnorm = Lnorm;
            double prevd0 = d0;
            TM_ali = standard_TMscore(xa, ya, xlen, ylen, invmap, L_ali, rmsd_ali);
            D0_MIN = prevD0_MIN;
            Lnorm = prevLnorm;
            d0 = prevd0;

            TM = detailed_search_standard(xa, ya, xlen, ylen, invmap, t, u, 40, 8, local_d0_search, true);
            if (TM > TMmax)
            {
                TMmax = TM;
                for (i = 0; i<ylen; i++)
                    invmap0[i] = invmap[i];
            }
            TM = DP_iter(xa, ya, xlen, ylen, t, u, invmap, 0, 2, (fast_opt)?2:30, local_d0_search);// Different from get_initial, get_initial_ss and get_initial_ssplus
            if (TM>TMmax)
            {
                TMmax = TM;
                for (i = 0; i<ylen; i++)
                {
                    invmap0[i] = invmap[i];
                }
            }
        }
    }



    //*******************************************************************//
    //    The alignment will not be changed any more in the following    //
    //*******************************************************************//
    //check if the initial alignment is generated approately
    bool flag=false;
    for(i=0; i<ylen; i++)
    {
        if(invmap0[i]>=0)
        {
            flag=true;
            break;
        }
    }
    if(!flag)
    {
        cout << "There is no alignment between the two proteins!" << endl;
        cout << "Program stop with no result!" << endl;
        return 1;
    }


    //********************************************************************//
    //    Detailed TMscore search engine --> prepare for final TMscore    //
    //********************************************************************//
    //run detailed TMscore search engine for the best alignment, and
    //extract the best rotation matrix (t, u) for the best alginment
    simplify_step=1;
    if (fast_opt) simplify_step=40;
    score_sum_method=8;
    TM = detailed_search_standard(xa, ya, xlen, ylen, invmap0, t, u, simplify_step, score_sum_method, local_d0_search, false);


    //select pairs with dis<d8 for final TMscore computation and output alignment
    int n_ali8, k=0;
    int n_ali=0;
    int *m1, *m2;
    double d;
    m1=new int[xlen]; //alignd index in x
    m2=new int[ylen]; //alignd index in y
    do_rotation(xa, xt, xlen, t, u);
    k=0;
    for(int j=0; j<ylen; j++)
    {
        i=invmap0[j];
        if(i>=0)//aligned
        {
            n_ali++;
            d=sqrt(dist(&xt[i][0], &ya[j][0]));
            if (d <= score_d8 || (I_opt == true))
            {
                m1[k]=i;
                m2[k]=j;

                xtm[k][0]=xa[i][0];
                xtm[k][1]=xa[i][1];
                xtm[k][2]=xa[i][2];

                ytm[k][0]=ya[j][0];
                ytm[k][1]=ya[j][1];
                ytm[k][2]=ya[j][2];

                r1[k][0] = xt[i][0];
                r1[k][1] = xt[i][1];
                r1[k][2] = xt[i][2];
                r2[k][0] = ya[j][0];
                r2[k][1] = ya[j][1];
                r2[k][2] = ya[j][2];

                k++;
            }
        }
    }
    n_ali8=k;

    double rmsd0 = 0.0;
    Kabsch(r1, r2, n_ali8, 0, &rmsd0, t, u);// rmsd0 is used for final output, only recalculate rmsd0, not t & u
    rmsd0 = sqrt(rmsd0 / n_ali8);


    //****************************************//
    //              Final TMscore             //
    //    Please set parameters for output    //
    //****************************************//
    double rmsd;
    double t0[3], u0[3][3];
    double TM1, TM2;
    double d0_out=5.0;
    simplify_step=1;
    score_sum_method=0;

    double d0_0, TM_0;
    double Lnorm_0=ylen;


    //normalized by length of structure A
    parameter_set4final(Lnorm_0);
    d0A=d0;
    d0_0=d0A;
    local_d0_search = d0_search;
    TM1 = TMscore8_search(xtm, ytm, n_ali8, t0, u0, simplify_step, score_sum_method, &rmsd, local_d0_search);
    TM_0 = TM1;

    //normalized by length of structure B
    parameter_set4final(xlen+0.0);
    d0B=d0;
    local_d0_search = d0_search;
    TM2 = TMscore8_search(xtm, ytm, n_ali8, t, u, simplify_step, score_sum_method, &rmsd, local_d0_search);


    if(a_opt)
    {
        //normalized by average length of structures A, B
        Lnorm_0=(xlen+ylen)*0.5;
        parameter_set4final(Lnorm_0);
        d0a=d0;
        d0_0=d0a;
        local_d0_search = d0_search;

        TM3 = TMscore8_search(xtm, ytm, n_ali8, t0, u0, simplify_step, score_sum_method, &rmsd, local_d0_search);
        TM_0=TM3;
    }
    if(u_opt)
    {
        //normalized by user assigned length
        parameter_set4final(Lnorm_ass);
        d0u=d0;
        d0_0=d0u;
        Lnorm_0=Lnorm_ass;
        local_d0_search = d0_search;
        TM4 = TMscore8_search(xtm, ytm, n_ali8, t0, u0, simplify_step, score_sum_method, &rmsd, local_d0_search);
        TM_0=TM4;
    }
    if(d_opt)
    {
        //scaled by user assigned d0
        parameter_set4scale(ylen, d0_scale);
        d0_out=d0_scale;
        d0_0=d0_scale;
        //Lnorm_0=ylen;
        Lnorm_d0=Lnorm_0;
        local_d0_search = d0_search;
        TM5 = TMscore8_search(xtm, ytm, n_ali8, t0, u0, simplify_step, score_sum_method, &rmsd, local_d0_search);
        TM_0=TM5;
    }

    /* print result */
    if (outfmt_opt==0) print_version();
    output_results(xname, yname, xlen, ylen, t0, u0, TM1, TM2, rmsd0, d0_out,
        m1, m2, n_ali8, n_ali, TM_0, Lnorm_0, d0_0, fname_matrix,
        dir1_opt, dir2_opt, outfmt_opt, ter_opt);

    /* free memory */
    delete [] invmap0;
    delete [] invmap;
    delete [] m1;
    delete [] m2;
    return 0; // zero for no exception
}
