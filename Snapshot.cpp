#include "Snapshot.h"
#include <linux/perf_event.h>
#define ARMA_DONT_PRINT_ERRORS
#include <armadillo>
#include <boost/math/distributions.hpp>

Snapshot::Snapshot() : cycles( PERF_COUNT_HW_CPU_CYCLES ),
                       instructions( PERF_COUNT_HW_INSTRUCTIONS ),
                       cachemisses( PERF_COUNT_HW_CACHE_MISSES ),
                       branchmisses( PERF_COUNT_HW_BRANCH_MISSES )
{}

Snapshot::~Snapshot()
{}

void Snapshot::start() {
    cycles.start();
    instructions.start();
    cachemisses.start();
    branchmisses.start();
}

void Snapshot::stop( const std::string& evname, uint64_t numitems, uint64_t numiterations ) {
    if ( numiterations==0 ) return;
    Sample samp;
    samp.numitems = numitems;
    samp.cycles = double(cycles.stop())/numiterations;
    samp.instructions = double(instructions.stop())/numiterations;
    samp.cachemisses = double(cachemisses.stop())/numiterations;
    samp.branchmisses = double(branchmisses.stop())/numiterations;
    samples[evname].push_back( samp );
}

static std::vector<uint32_t> calcMask( uint32_t num ) {
    std::vector<uint32_t> ixvec;
    for ( unsigned j=0; num>0; ++j, num>>=1 ) {
        if ( num&1 !=0 ) ixvec.push_back( j );
    }
    return ixvec;
}

// I should really spawn this into a LinearModel class
struct RegResults {
    bool ok;
    arma::mat C;
    arma::colvec b;
    arma::colvec sol;
    arma::colvec res;
    arma::colvec serr;
    arma::colvec tval;
    arma::colvec pval;

    double fval;
    double fpval;
    double rsq;
    double rsqadj;
    double loglik;
    double aic;
    double bic;

    bool solve();
};

static bool calcModel( uint32_t modelnum,
                       const arma::mat& C,
                       const arma::vec& b,
                       RegResults& reg )
{
    auto ixvec = calcMask( modelnum );
    uint32_t numcols = ixvec.size();
    uint32_t numrows = C.n_rows;
    reg.b = b;
    reg.C.resize( numrows, numcols );
    for ( uint32_t j=0; j<numcols; ++j ) {
        reg.C.col(j) = C.col( ixvec[j] );
    }
    return reg.solve();
}

static arma::colvec cdf( const arma::colvec& x, uint32_t ndof ) {
    unsigned nrows = x.n_rows;
    arma::colvec y( nrows );
    boost::math::students_t st( ndof );
    for ( unsigned j=0; j<nrows; ++j ) {
        y(j) = cdf( st, x(j) );
    }
    return y;
}

bool RegResults::solve()
{
    // Dimensionality of the problem
    uint32_t nobs = C.n_rows;
    uint32_t ncoef= C.n_cols;

    // Initialize all metrics to NAN
    rsq = rsqadj = fval = fpval = loglik = aic = bic
        = std::numeric_limits<double>::quiet_NaN();

    // Check dimensions
    if ( nobs<=ncoef ) return false;

    // Solve system with LSQ
    ok = arma::solve( sol, C, b);
    if ( not ok ) return false;

    try {
        // Residuals
        res = b - C*sol;

        // Degrees of freedom
        uint32_t ndof = nobs - ncoef;

        // Variance of residuals
        double s2 = arma::dot( res, res )/ndof;

        // Standard errors
        serr = arma::sqrt( s2 * arma::diagvec( arma::pinv(C.t()*C) ));

        // t-values and respective p-values
        tval = sol / serr;
        pval = (1 - cdf( arma::abs(tval), ndof ))*2;

        // R-squared measures
        rsq = 1 - arma::dot(res,res)/arma::dot(b,b);
        rsqadj = 1-(1-rsq)*((nobs-1)/(nobs-ncoef));

        // Compute F-value and respective probability for model selection
        fval = (rsq/(ncoef-1))/((1-rsq)/ndof);
        boost::math::fisher_f  ff( ncoef-1, nobs-ncoef );
        fpval = 1 - cdf(ff,fval);

        // log likelihood for model selection with Akaike information coefficients
        loglik = -(nobs*0.5)*(1+log(2*M_PI))
            - (nobs/2.)*log( arma::dot(res,res)/nobs );
        aic = -(2.*loglik)/nobs + double(2*ncoef)/nobs;
        bic = -(2.*loglik)/nobs + double(ncoef*log(nobs))/nobs;
    }
    catch( ... ) {
        // bad luck
        return false;
    }
    return true;
}

void Snapshot::summary( const std::string& header, FILE* f ) {

    // cycle through events map
    for ( const auto& ism: samples )
    {
        std::string evname = ism.first;
        const std::vector<Sample>& svec( ism.second );

        // fill in data matrix with all points collected
        uint32_t numpoints = svec.size();
        double suminstr = 0;
        double sumbranches = 0;
        double sumcycles = 0;
        arma::mat C( numpoints, 7 );
        arma::vec b( numpoints );
        for ( unsigned j=0; j<numpoints; ++j ) {
            const Sample& sm( svec[j] );
            C(j,0) = 1;
            C(j,1) = double(sm.cachemisses);
            C(j,2) = double(sm.branchmisses);
            C(j,3) = log(sm.numitems);
            C(j,4) = sm.numitems;
            C(j,5) = sm.numitems*log(sm.numitems);
            C(j,6) = sm.numitems*sm.numitems;
            b(j) = double(sm.cycles);
            suminstr += double(sm.instructions);
            sumcycles += double(sm.cycles);
            sumbranches += double(sm.branchmisses);
        }
        double cycinstr = suminstr>0 ? sumcycles/suminstr : -1;
        double cycbranch = sumbranches>0 ? sumcycles/sumbranches : -1;

        RegResults bestreg;
        bool found = false;
        uint32_t bestmodel = 0;
        for ( uint32_t np = 0; np<4; np++ ) {
            for ( uint32_t k=1; k<=7; ++k )
            {
                uint32_t modelnum = k + ( 1 << (np+3) );
                RegResults reg;
                if ( calcModel( modelnum, C, b, reg ) ) {
                    if ( reg.sol.min()<0 )
                        continue;
                    if ( reg.pval.max()>0.05 )
                        continue;

                    if ( (not found) or (reg.aic<bestreg.aic) ) {
                        bestreg = reg;
                        bestmodel = modelnum;
                        found = true;
                    }
                }
            }
        }

        std::vector<std::string> colnames  = { "Constant",
                                               "CacheMisses",
                                               "BranchMisses",
                                               "Log(N)", "N",
                                               "N*Log(N)", "N^2" };
        fprintf( f, "%s, Event:%s, Cyc/Ins:%3.2f Cyc/Bch:%3.2f Points:%d Rsq:%5.2f F:%f LL:%f aic:%f bic:%f \n",
                 header.c_str(), evname.c_str(),
                 cycinstr, cycbranch, numpoints,
                 bestreg.rsq, bestreg.fpval,
                 bestreg.loglik, bestreg.aic, bestreg.bic );

        auto mask = calcMask( bestmodel );
        for ( unsigned j=0; j<mask.size(); ++j ) {
            fprintf( f, "   Term: %-12s  p:%7.5f coef:%g\n",
                     colnames[mask[j]].c_str(),
                     bestreg.pval(j),
                     bestreg.sol(j) );
        }
    }
}
