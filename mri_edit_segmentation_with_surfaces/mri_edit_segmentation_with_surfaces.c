
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "macros.h"
#include "error.h"
#include "diag.h"
#include "proto.h"
#include "mrisurf.h"
#include "mri.h"
#include "macros.h"
#include "fio.h"
#include "mrishash.h"
#include "cma.h"
#include "mrisegment.h"
#include "colortab.h"
#include "gca.h"

static char vcid[] = "$Id: mri_edit_segmentation_with_surfaces.c,v 1.12 2006/05/09 21:05:32 fischl Exp $";

int main(int argc, char *argv[]) ;

double MRIlabelMean(MRI *mri, MRI  *mri_labels, int x, int y, int z, int whalf, int label) ;
static int  get_option(int argc, char *argv[]) ;
static void usage_exit(void) ;
static void print_usage(void) ;
static void print_help(void) ;
static void print_version(void) ;
static int relabel_hypointensities(MRI *mri, MRI_SURFACE *mris, int right,
																	 GCA *gca, TRANSFORM *transform) ;
/*static int relabel_hypointensities_neighboring_gray(MRI *mri) ;*/
#if 0
static int  edit_unknowns(MRI *mri_aseg, MRI *mri) ;
static int edit_hippocampal_complex(MRI *mri, MRI_SURFACE *mris, int right, char *annot_name, float *thickness) ;
/*static int edit_hippocampus(MRI *mri) ;*/
static int edit_calcarine(MRI *mri, MRI_SURFACE *mris, int right) ;
#endif

static char *annot_name = "aparc.annot" ;

char *Progname ;

static char *label_name = NULL ;
static char *annotation_name = NULL ;

static char *surf_name = "white" ;

static MRI *mri_vals = NULL ;
static GCA *gca = NULL ;
static TRANSFORM *transform = NULL ;

int
main(int argc, char *argv[])
{
  char          **av, *hemi, fname[STRLEN], *in_fname,
                *in_aseg_name, *out_aseg_name, *surf_dir ;
  int           ac, nargs, h, i, ninputs, input ;
  MRI_SURFACE   *mris ;
  MRI           *mri_aseg, *mri_tmp = NULL, *mri_inputs = NULL ;
	float         *thickness ;

  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  ac = argc ;
  av = argv ;
  for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++)
  {
    nargs = get_option(argc, argv) ;
    argc -= nargs ;
    argv += nargs ;
  }

  if (argc < 4)
    usage_exit() ;

	in_aseg_name = argv[1] ;
  surf_dir = argv[2] ;
	out_aseg_name = argv[argc-1] ;
	ninputs = argc-4 ;
	printf("reading %d input volumes\n", ninputs) ;

  for (input = 0 ; input < ninputs ; input++)
	{
		in_fname = argv[3+input] ;
		printf("reading input volume from %s...\n", in_fname) ;
		mri_tmp = MRIread(in_fname) ;
		if (!mri_tmp)
			ErrorExit(ERROR_NOFILE, "%s: could not read input MR volume from %s",
								Progname, in_fname) ;

		if (input == 0)
		{
			mri_inputs =
				MRIallocSequence(mri_tmp->width, mri_tmp->height, mri_tmp->depth,
												 mri_tmp->type, ninputs) ;
			if (!mri_inputs)
				ErrorExit
					(ERROR_NOMEMORY,
					 "%s: could not allocate input volume %dx%dx%dx%d",
					 mri_tmp->width, mri_tmp->height, mri_tmp->depth,ninputs) ;
			MRIcopyHeader(mri_tmp, mri_inputs) ;
		}

		MRIcopyFrame(mri_tmp, mri_inputs, 0, input) ;
		MRIfree(&mri_tmp) ;
	}
  mri_aseg = MRIread(in_aseg_name) ;
  if (!mri_aseg)
    ErrorExit(ERROR_NOFILE, "%s: could not read input segmentation %s", Progname, in_aseg_name) ;

	for (i = 0 ; i < 2 ; i++)
	{
		for (h = 0 ; h <= 1 ; h++)
		{
			if (h == 0)
				hemi = "lh" ;
			else
				hemi = "rh" ;
			sprintf(fname, "%s/%s.%s", surf_dir, hemi, surf_name)  ;
			printf("reading input surface %s...\n", fname) ;
			mris = MRISread(fname) ;
			if (!mris)
				ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s",
									Progname, fname) ;
			MRISsaveVertexPositions(mris, ORIGINAL_VERTICES) ;
			MRIScomputeMetricProperties(mris) ;
			MRISsmoothSurfaceNormals(mris, 10) ;   /* remove kinks in surface */
			thickness = MRISreadCurvatureVector(mris, "thickness") ;
			if (thickness == NULL)
				ErrorExit(ERROR_NOFILE, "%s: could not read thickness file for %s\n",Progname,fname) ;


			if (MRISreadAnnotation(mris, annot_name) != NO_ERROR)
				ErrorExit(ERROR_NOFILE, "%s: could not read annotation file %s for hemi %s\n",
									Progname, annot_name, hemi) ;
		
			if (mris->ct == NULL)  /* color table not in annotation file */
			{
				char *cp, fname[STRLEN] ;
				cp = getenv("FREESURFER_HOME") ;
				sprintf(fname, "%s/Simple_surface_labels2002.txt", cp) ;
				printf("reading colortable from %s...\n", fname) ;
				mris->ct = CTABread(fname) ;
				if (!mris->ct)
					ErrorExit(ERROR_NOFILE, "%s: could not read color table from %s",Progname, fname) ;
			}

#if 0
			printf("%s: removing ventricular voxels in calcarine...\n", hemi) ;
			edit_calcarine(mri_aseg, mris, h) ;
			printf("%s: editing hippocampal complex...\n", hemi) ;
			edit_hippocampal_complex(mri_aseg, mris, h, annot_name, thickness) ;
#endif
			printf("%s: relabeling hypointensities...\n", hemi) ;
			relabel_hypointensities(mri_aseg, mris, h, gca, transform) ;
			MRISfree(&mris) ; free(thickness) ;
		}
		if (transform && i == 0)
		{
			TransformInvert(transform, mri_inputs) ;
			mri_tmp = GCAlabelWMandWMSAs(gca, mri_inputs, mri_aseg, mri_tmp, transform) ;
			MRIfree(&mri_aseg) ;
			mri_aseg = mri_tmp ;
		}
	}

#if 0
	/*	edit_hippocampus(mri_aseg) ;*/
	if (mri_vals)
		edit_unknowns(mri_aseg, mri_vals) ;
#endif

	printf("writing modified segmentation to %s...\n", out_aseg_name) ;
	MRIwrite(mri_aseg, out_aseg_name) ;
  exit(0) ;
  return(0) ;  /* for ansi */
}

/*----------------------------------------------------------------------
            Parameters:

           Description:
----------------------------------------------------------------------*/
static int
get_option(int argc, char *argv[])
{
  int  nargs = 0 ;
  char *option ;
  
  option = argv[1] + 1 ;            /* past '-' */
  if (!stricmp(option, "-help"))
    print_help() ;
  else if (!stricmp(option, "-version"))
    print_version() ;
  else if (!stricmp(option, "-annot"))
	{
		annot_name = argv[2] ;
		nargs = 2 ;
		printf("using annotation file %s...\n", annot_name) ;
	}
  else if (!stricmp(option, "debug_voxel"))
	{
		Ggca_x = Gx = atoi(argv[2]) ;
		Ggca_y = Gy = atoi(argv[3]) ;
		Ggca_z = Gz = atoi(argv[4]) ;
		nargs = 3 ;
		printf("debugging voxel (%d, %d, %d)\n", Gx, Gy, Gz) ;
	}
  else if (!stricmp(option, "MRI"))
	{
		printf("reading MRI volume from %s...\n", argv[2]) ;
		mri_vals = MRIread(argv[2]) ;
		if (!mri_vals)
			ErrorExit(ERROR_NOFILE, "%s: could not read MRI volume from %s",
								Progname, argv[2]) ;
		nargs = 1 ;
	}
  else if (!stricmp(option, "GCA"))
	{
		printf("reading GCA from %s, and xform from %s...\n", argv[2], argv[3]) ;
		gca = GCAread(argv[2]) ;
		if (!gca)
			ErrorExit(ERROR_NOFILE, "%s: could not read GCA from %s",
								Progname, argv[2]) ;
		transform = TransformRead(argv[3]) ;
		if (!transform)
			ErrorExit(ERROR_NOFILE, "%s: could not read transform from %s",
								Progname, argv[3]) ;
		nargs = 2 ;
	}
  else switch (toupper(*option))
  {
  case 'L':
    label_name = argv[2] ;
    nargs = 1 ;
    fprintf(stderr, "limiting computations to label %s.\n", label_name) ;
    break ;
  case 'A':
    annotation_name = argv[2] ;
    nargs = 1 ;
    fprintf(stderr, "computing statistics for each annotation in %s.\n", 
            annotation_name) ;
    break ;
  case '?':
  case 'U':
    print_usage() ;
    exit(1) ;
    break ;
  default:
    fprintf(stderr, "unknown option %s\n", argv[1]) ;
    exit(1) ;
    break ;
  }

  return(nargs) ;
}

static void
usage_exit(void)
{
  print_help() ;
  exit(1) ;
}

static void
print_usage(void)
{
  fprintf(stderr, 
          "usage: %s [options] <subject name> <hemi> [<surface name>]\n", 
          Progname) ;
}

static void
print_help(void)
{
  print_usage() ;
  fprintf(stderr, 
          "\nThis program measures a variety of anatomical properties\n") ;
  fprintf(stderr, "\nvalid options are:\n\n") ;
  fprintf(stderr,
          "-l <label file>              - limit calculations to specified "
          "label\n") ;
  fprintf(stderr,
          "-a <annotation file>         - compute properties for each label\n"
          "                               in the annotation file separately"
          "\n") ;
  exit(1) ;
}

static void
print_version(void)
{
  fprintf(stderr, "%s\n", vcid) ;
  exit(1) ;
}

static int
relabel_hypointensities(MRI *mri, MRI_SURFACE *mris, int right, GCA *gca, TRANSFORM *transform)
{
	int              x, y, z, label, changed, n ;
	GCA_PRIOR        *gcap ;
	MRI              *mri_inside, *mri_inside_eroded, *mri_inside_dilated ;
	MRIS_HASH_TABLE *mht ;
	VERTEX           *v ;
	float            dx, dy, dz, dot, dist ;
	Real             xw, yw, zw ;

	mri_inside = MRIclone(mri, NULL) ;
	MRISeraseOutsideOfSurface(0.5, mri_inside, mris, 128) ;
	mri_inside_eroded = MRIerode(mri_inside, NULL) ;
	mri_inside_dilated = MRIdilate(mri_inside, NULL) ;
	mht = MHTfillVertexTableRes(mris,NULL, CURRENT_VERTICES, 8.0f) ;
	if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
	{
		char fname[STRLEN] ;
		sprintf(fname, "%s_inside.mgz", right?"rh":"lh") ;
		MRIwrite(mri_inside, fname) ;
	}
	if (gca)
		TransformInvert(transform, mri) ;
	for (changed = x = 0 ; x < mri->width ; x++)
	{
		for (y = 0 ; y < mri->height ; y++)
		{
			for (z = 0 ; z < mri->depth ; z++)
			{
				if (x == Gx && y == Gy && z == Gz)
					DiagBreak() ;
				label = MRIvox(mri, x, y, z) ;
				if (right &&
						((label != Right_Cerebral_Cortex) && (label != Right_WM_hypointensities)))
					continue ;
				if (right == 0 &&
						((label != Left_Cerebral_Cortex) && (label != Left_WM_hypointensities)))
					continue ;

				if (MRIneighbors(mri, x, y, z, Unknown) >= 3) /* avoid stuff outside of brain in a sea of unknown */
					continue ;

				MRIvoxelToSurfaceRAS(mri, x, y, z, &xw, &yw, &zw) ;
				v = MHTfindClosestVertexInTable(mht, mris, xw, yw, zw) ;
				if (v == NULL)  // no vertices within range - either way inside or way outside
				{
					if  (MRIgetVoxVal(mri_inside, x, y, z, 0) > 0)
						dot = -1 ;
					else
						dot = 1 ;
					dist = 1000 ;
				}
				else  // see where we are relative to closest vertex
				{
					dx = xw - v->x ; dy = yw - v->y ; dz = zw - v->z ; 
					dot = v->nx*dx + v->ny*dy + v->nz*dz ;
					dist = sqrt(dx*dx+dy*dy+dz*dz) ;
				}

				if (dot > 0 && (MRIgetVoxVal(mri_inside_eroded, x, y, z, 0) > 0))  // should be inside
					dot *= -1 ;
				else if (dot < 0 && (MRIgetVoxVal(mri_inside_dilated, x, y, z, 0) == 0)) // should be outside
					dot *= -1 ;

				if (x == Gx && y == Gy && z == Gz)
					printf("(%d, %d, %d), label %s, dist = %2.1f, dot = %2.1f\n", Gx, Gy, Gz, 
								 cma_label_to_name(label), dist, dot) ;
				switch (label)
				{
				case Left_Cerebral_Cortex:
				case Right_Cerebral_Cortex:  // check to see if it's inside ribbon and change it to hypointensity
					if (dot < 0 && dist > 2)
					{
						if (x == Gx && y == Gy && z == Gz)
							printf("inside gray/white boundary, changing to hypointensity...\n") ;
						changed++ ;
						MRIvox(mri, x, y, z) = right ? Right_WM_hypointensities : Left_WM_hypointensities ;
					}
					break ;
				case Left_WM_hypointensities:
				case Right_WM_hypointensities: // check to see if it's outside ribbon and change it to gm
					if (gca)  // if we have a gca, check to make sure gm is possible here
					{
						int found, xk, yk, zk, xi, yi, zi, whalf, found_ven ;
						double dist ;
						
						// don't change things on the walls of the ventricles
						gcap = getGCAP(gca, mri, transform, x, y, z) ;
						found = 0 ;
						for (n = 0 ; n < gcap->nlabels ; n++)
						{
							if (IS_LAT_VENT(gcap->labels[n]) && (gcap->priors[n] > 0.5))
							{
								found = 1 ;
							}
							if (IS_CAUDATE(gcap->labels[n]))
							{
								found = 1 ;
							}
						}
						if (found == 1)  // too near ventricle - can't be gm
						{
							if (x == Gx && y == Gy && z == Gz)
								printf("too near ventricles and/or caudate - not changing\n") ;
							continue ;
						}

						// now see if gm is possible in this region 
						// and make sure no ventricle in nbhd
						whalf = gca->prior_spacing ;
						for (found_ven = found = 0, xk =-whalf ; !found_ven && xk <= whalf ; xk++)
						{
							xi = mri->xi[x+xk] ;
							for (yk =-whalf ; !found_ven && yk <= whalf ; yk++)
							{
								yi = mri->yi[y+yk] ;
								for (zk =-whalf ; !found_ven && zk <= whalf ; zk++)
								{
									zi = mri->zi[z+zk] ;

									dist = sqrt(xk*xk+yk*yk+zk*zk) ;
									if (dist > gca->prior_spacing+1)
										continue ;
									label = MRIvox(mri, xi, yi, zi) ;
									gcap = getGCAP(gca, mri, transform, xi, yi, zi) ;
									for (n = 0 ; n < gcap->nlabels ; n++)
									{
										if (gcap->labels[n] == Left_Lateral_Ventricle ||
												label == Right_Lateral_Ventricle)
										{
											found_ven = 1 ;
											if (x == Gx && y == Gy && z == Gz)
												printf("ventricle label found at (%d, %d, %d)\n",
															 xi, yi, zi) ;
											break ;
										}
										if (IS_GM(gcap->labels[n]) && gcap->priors[n] > 0.1)
										{
											if (x == Gx && y == Gy && z == Gz)
												printf("possible gray matter found at (%d, %d, %d)\n",
															 xi, yi, zi) ;
											found = 1  ;
										}
									}
								}
							}
						}
						if ((found == 0) || (found_ven == 1))
						{
							if (x == Gx && y == Gy && z == Gz)
								printf("gray matter not possible at this location...\n") ;
							continue ;   // don't change it
						}
					}

					// if it's outside, or just inside change it to gm
					if (dot > 0 || (dot < 0 &&  dist < 1))
					{
						changed++ ;
						if (x == Gx && y == Gy && z == Gz)
							printf("outside ribbon: changing to cerebral cortex...\n") ;
						MRIvox(mri, x, y, z) = right ? Right_Cerebral_Cortex : Left_Cerebral_Cortex ;
					}
					break ;
				}
			}
		}
	}

	printf("%d voxels changed to hypointensity...\n", changed) ;
	MHTfree(&mht) ; MRIfree(&mri_inside) ;  MRIfree(&mri_inside_eroded) ; MRIfree(&mri_inside_dilated) ; 
	return(NO_ERROR) ;
}
#if 0
int
relabel_hypointensities_neighboring_gray(MRI *mri)
{
	int    x, y, z, label, changed, i ;
	MRI    *mri_tmp = NULL ;

	for (changed = i = 0 ; i < 2 ; i++)
	{
		mri_tmp = MRIcopy(mri, mri_tmp) ;
		for (x = 0 ; x < mri->width ; x++)
		{
			for (y = 0 ; y < mri->height ; y++)
			{
				for (z = 0 ; z < mri->depth ; z++)
				{
					label = MRIvox(mri_tmp, x, y, z) ;
					if (label != WM_hypointensities)
						continue ;
					if (MRIneighbors(mri_tmp, x, y, z, Left_Cerebral_Cortex) > 0)
					{
						MRIvox(mri, x, y, z) = Left_Cerebral_Cortex ;
						changed++ ;
					}
					else  if (MRIneighbors(mri_tmp, x, y, z, Right_Cerebral_Cortex) > 0)
					{
						MRIvox(mri, x, y, z) = Right_Cerebral_Cortex ;
						changed++ ;
					}
				}
			}
		}
	}
	printf("%d hypointense voxels neighboring cortex changed\n", changed) ;
	return(NO_ERROR) ;
}
#endif
#define MEDIAL_WALL            41/* only for Simple_surface_labels2002.txt - will have to modify for CMA */
#define FUSIFORM_GYRUS         17
#define LINGUAL_SULCUS         18
#define LINGUAL_SULCUS2        66
#define PARAHIPPOCAMPAL_GYRUS  19
#define TEMPORAL_POLE          43
#define COLLATERAL_SULCUS_ANT  52
#define COLLATERAL_SULCUS_POS  53
#define SUP_TEMP_GYRUS_POLE    33
#define CALCARINE_SULCUS       44
#define MIN_DIST 0.5
#define MAX_DIST 2

#define IS_INF_TO_HIPPO(index)  (index == PARAHIPPOCAMPAL_GYRUS || index == LINGUAL_SULCUS || index == LINGUAL_SULCUS2 || index == COLLATERAL_SULCUS_ANT || index == COLLATERAL_SULCUS_POS)


#if 0
static int
edit_hippocampal_complex(MRI *mri, MRI_SURFACE *mris, int right, char *annot_name, float *thickness)
{
  MRIS_HASH_TABLE *mht ;
  int              x, y, z, label, changed, index, total_changed, i, vno, yi, dont_use, ystart, ind ;
  Real             xw, yw, zw, xv, yv, zv ;
  VERTEX           *v ;
  float            dx, dy, dz, dot, dist ;
  MRI              *mri_changed, *mri_above_para, *mri_below_para, *mri_above_inf_lat_vent, *mri_inside, *mri_aparc ;
  
  mri_inside = MRIclone(mri, NULL) ;
  mri_changed = MRIclone(mri, NULL) ;
  mri_above_para = MRIclone(mri, NULL) ;
  mri_below_para = MRIclone(mri, NULL) ;
  mri_above_inf_lat_vent = MRIclone(mri, NULL) ;
  mri_aparc = MRIclone(mri, NULL) ;
  MRISeraseOutsideOfSurface(0.0, mri_inside, mris, 128) ;
  MRIdilate(mri_inside, mri_inside) ;
  
  /* build mask volume of stuff superior to inf lat vent */
  for (changed = x = 0 ; x < mri->width ; x++)
  {
    for (y = 0 ; y < mri->height ; y++)
    {
      for (z = 0 ; z < mri->depth ; z++)
      {
				if (x == Gx && y == Gy && z == Gz)
					DiagBreak() ;
				label = MRIvox(mri, x, y, z) ;
				if ((label != Right_Inf_Lat_Vent) && (label != Left_Inf_Lat_Vent))
					continue ;
				for (yi = y-1 ; yi >= 0 ; yi--)
					MRIvox(mri_above_inf_lat_vent, x, yi, z) = 128 ;
      }
    }
  }
  MRIclose(mri_above_inf_lat_vent, mri_above_inf_lat_vent) ;

  /* build volume with parcellation labels in it */
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    index = CTABannotationToIndex(mris->ct, v->annotation) ;
    for (dist = 0 ; dist < 1 /*thickness[vno]*/ ; dist = dist + 0.25)
    {
      xw = v->x +  v->nx*dist ; 
      yw = v->y +  v->ny*dist ; 
      zw = v->z +  v->nz*dist ; 
      // MRIworldToVoxel(mri, xw, yw, zw, &xv, &yv, &zv) ;
      MRIsurfaceRASToVoxel(mri, xw, yw, zw, &xv, &yv, &zv) ;
      x = nint(xv) ; y = nint(yv) ; z = nint(zv) ;
      MRIvox(mri_aparc, x, y, z) = index ;
    }
  }		
  
  
  /* build two masks for stuff that is above (below) the parahippocampal gyrus */
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    MRISvertexToVoxel(mris, v, mri_above_para, &xw, &yw, &zw) ;
    index = CTABannotationToIndex(mris->ct, v->annotation) ;
    if ((index == LINGUAL_SULCUS || index == LINGUAL_SULCUS2) &&
				sqrt(SQR(xw-Gx)+SQR(yw-Gy)+SQR(zw-Gz)) < 3)
      DiagBreak() ;
    if (IS_INF_TO_HIPPO(index) == 0)
      continue ;
#if 0
    if (index != PARAHIPPOCAMPAL_GYRUS && index != LINGUAL_SULCUS && index != LINGUAL_SULCUS2 &&
				index != COLLATERAL_SULCUS_ANT && index != COLLATERAL_SULCUS_POS/* && index != TEMPORAL_POLE*/)
      continue ;
#endif
    x = nint(xw) ; y = nint(yw) ; z = nint(zw) ;
    
    /* check to see if these that we are inferior to a medial wall label */
    dont_use = 0 ;
    for (yi = y-1 ; yi >= 0 ; yi--)
    {
      if (x == Gx && yi == Gy && z == Gz)
				DiagBreak() ;
      
      if (MRIvox(mri_aparc, x, yi, z) != MEDIAL_WALL &&
					MRIvox(mri_aparc, x, yi, z) != index &&
					MRIvox(mri_aparc, x, yi, z) != 0)
      {
				dont_use = 1 ;
				break ;
      }
      if (MRIvox(mri_aparc, x, yi, z) == MEDIAL_WALL)
				break ;
    }
    
    for (yi = y+1 ; yi < mri->height ; yi++)
      if ((MRIvox(mri_aparc, x, yi, z) > 0) &&
					(MRIvox(mri_aparc, x, yi, z) != index))
      {
				if (x == Gx && yi == Gy && z == Gz)
					DiagBreak() ;
				if ((index == PARAHIPPOCAMPAL_GYRUS || index == LINGUAL_SULCUS || index == LINGUAL_SULCUS2) &&
						MRIvox(mri_aparc, x, yi, z) == FUSIFORM_GYRUS)
					continue ;
				dont_use = 1 ;
				break ;
      }
    
    /* search superior - if a different label that can be inf to hippo, don't use this one */
    ind = index ;
    for (yi = y-1 ; yi >= 0 ; yi--)
    {
      /* crossed unknown - not part of same surface location */
      if (MRIvox(mri_aparc, x, yi, z) == 0)  
				ind = 0 ;
      if (IS_INF_TO_HIPPO(MRIvox(mri_aparc, x, yi, z)) && MRIvox(mri_aparc, x, yi, z) != ind)
      {
				dont_use = 1 ;
				break ;
      }
    }
    
    if (dont_use)
      continue ;
    
#if 0
    /* search for the end of this label and start filling there */
    for (ystart = y-1 ; ystart >= 0 ; ystart--)
    {
      if (MRIvox(mri_aparc, x, ystart, z) == 0 ||
					MRIvox(mri_aparc, x, ystart, z) == MEDIAL_WALL)
				break ;
    }
#else
    ystart = y ;
#endif
    
    for (yi = ystart-1 ; yi >= 0 ; yi--)
    {
      if (x == Gx && yi == Gy && z == Gz)
				DiagBreak() ;
      MRIvox(mri_above_para, x, yi, z) = 128 ;
    }
    for (yi = ystart+1 ; yi < mri->height ; yi++)
      MRIvox(mri_below_para, x, yi, z) = 128 ;
  }		
  
  MRIclose(mri_above_para, mri_above_para) ;
  MRIclose(mri_below_para, mri_below_para) ;
  
  {
    int              max_voxels, i, max_i, j ;
    MRI_SEGMENTATION  *mriseg ;
    MRI_SEGMENT       *mseg ;
    MRI_SEGMENT_VOXEL *msv ;
    
    mriseg = MRIsegment(mri_above_para, 1, 255) ;
    for (max_i = max_voxels = i = 0 ; i < mriseg->nsegments ; i++)
    {
      mseg = &mriseg->segments[i] ;
      if (mseg->nvoxels > max_voxels)
      {
				max_i = i ;
				max_voxels = mseg->nvoxels ;
      }
    }
    
    /* erase other segments */
    for (i = 0 ; i < mriseg->nsegments ; i++)
    {
      if (i == max_i)
				continue ;
      mseg = &mriseg->segments[i] ;
      for (j = 0 ; j < mseg->nvoxels ; j++)
      {
				msv = &mseg->voxels[j] ;
				MRIvox(mri_above_para, msv->x, msv->y, msv->z) = 0 ;
      }
    }
    MRIsegmentFree(&mriseg) ;
    
    mriseg = MRIsegment(mri_below_para, 1, 255) ;
    for (max_i = max_voxels = i = 0 ; i < mriseg->nsegments ; i++)
    {
      mseg = &mriseg->segments[i] ;
      if (mseg->nvoxels > max_voxels)
      {
				max_i = i ;
				max_voxels = mseg->nvoxels ;
      }
    }
    
    /* erase other segments */
    for (i = 0 ; i < mriseg->nsegments ; i++)
    {
      if (i == max_i)
				continue ;
      mseg = &mriseg->segments[i] ;
      for (j = 0 ; j < mseg->nvoxels ; j++)
      {
				msv = &mseg->voxels[j] ;
				MRIvox(mri_below_para, msv->x, msv->y, msv->z) = 0 ;
      }
    }
    
    MRIsegmentFree(&mriseg) ;
  }
  
  mht = MHTfillVertexTableRes(mris,NULL, CURRENT_VERTICES, 8.0f) ;
  
  /* find voxels that are below the parahippocampal gyrus and labeled as something that should
     be above (hippocampus, amygdala, inf lat vent), and change it's label to cortex.
  */
  for (changed = x = 0 ; x < mri->width ; x++)
  {
    for (y = 0 ; y < mri->height ; y++)
    {
      for (z = 0 ; z < mri->depth ; z++)
      {
				if (x == Gx && y == Gy && z == Gz)
					DiagBreak() ;
				if (MRIvox(mri_above_para, x, y, z) > 0)  /* above the parahippocampal gyrus */
					continue ;
				if (MRIvox(mri_below_para, x, y, z) == 0)  /* not below the parahippocampal gyrus */
					continue ;
	
				label = MRIvox(mri, x, y, z) ;
				/* only process amygdala and hippocampal voxels for the correct hemisphere */
				if ((right && ((label != Right_Hippocampus) && (label != Right_Amygdala) && (label != Right_Inf_Lat_Vent))) ||
						(!right && ((label != Left_Hippocampus) && (label != Left_Amygdala) && (label != Left_Inf_Lat_Vent))))
					continue ;
				MRIvoxelToSurfaceRAS(mri, x, y, z, &xw, &yw, &zw) ;
				v = MHTfindClosestVertexInTable(mht, mris, xw, yw, zw) ;
				if (v == NULL)
					continue ;
				index = CTABannotationToIndex(mris->ct, v->annotation) ;
#if 0
				if (index == PARAHIPPOCAMPAL_GYRUS)
				{
#if 1
					/* don't change voxels where the wm surface normal is pointing superiorly */
					if (v->nz >= 0)
					{
						if (dist > MAX_DIST)  /* don't process amygdala that is far from surface */
							continue ;
						if (((fabs(v->nz) > fabs(v->nx)) || (fabs(v->nz) > fabs(v->ny))))
							continue ;   /* this voxel is superior to wm surface */
						/* at this point, the wm vertex is running nearly inferior-superior, so use
							 laterality to determine which side of parahippo it is on */
						if (fabs(xw) - fabs(v->x) < MIN_DIST)  /* if it isn't clearly lateral to parahippo, don't change it */
							continue ;
	    
					}
					else if (((fabs(v->nz) < fabs(v->nx)) || (fabs(v->nz) < fabs(v->ny)))) /* not clearly inf or sup */
					{
						/* at this point, the wm vertex is running nearly inferior-superior, so use
							 laterality to determine which side of parahippo it is on */
						if (fabs(xw) - fabs(v->x) < MIN_DIST)  /* if it isn't clearly lateral to parahippo, don't change it */
							continue ;
						if ((((fabs(v->nz)) < fabs(v->nx)) && ((fabs(v->nz) < fabs(v->ny)))))
							continue ;  /* if it's not clearly pointing up, don't use it */
					}
#else
					if (zw > (v->z) && (fabs(zw-v->z) > MIN_DIST))
						continue ;  /* don't change wm that is superior to parahippocampal gyrus */
					if (fabs(zw-v->z) < MIN_DIST)  /* too close - make sure it is medial of wm */
					{
						if (fabs(xw) - fabs(v->x) < MIN_DIST)  /* if it isn't clearly lateral to parahippo, don't change it */
							continue ;
					}
#endif
				}
#endif
	
				if (x == Gx && y == Gy && z == Gz)
					printf("voxel (%d, %d, %d): label %d (%s), parc %d, changing to cortex...\n",
								 Gx, Gy, Gz, label, cma_label_to_name(label), index) ;
				if (right)
					MRIvox(mri, x, y, z) = Right_Cerebral_Cortex ;
				else
					MRIvox(mri, x, y, z) = Left_Cerebral_Cortex ;
				changed++ ;
				MRIvox(mri_changed, x, y, z) = 128 ;
      }
    }
  }


  /* look for points inside wm labeled as cortex  and change them to wm */
  for (x = 0 ; x < mri->width ; x++)
  {
    for (y = 0 ; y < mri->height ; y++)
    {
      for (z = 0 ; z < mri->depth ; z++)
      {
				if (x == Gx && y == Gy && z == Gz)
					DiagBreak() ;
				if (MRIvox(mri_inside, x, y, z) == 0)  
					continue ;   /* not really inside */
				label = MRIvox(mri, x, y, z) ;
	
				/* only process cortex voxels for the correct hemisphere */
				if ((right && ((label != Right_Cerebral_Cortex) && (label != Right_Hippocampus))) ||
						(!right && ((label != Left_Cerebral_Cortex) && (label != Left_Hippocampus))))
					continue ;
				MRIvoxelToSurfaceRAS(mri, x, y, z, &xw, &yw, &zw) ;
				v = MHTfindClosestVertexInTable(mht, mris, xw, yw, zw) ;
				if (v == NULL)
					continue ;
				index = CTABannotationToIndex(mris->ct, v->annotation) ;
				/* only thin temporal wm */
				if ((index != MEDIAL_WALL) && (index != LINGUAL_SULCUS) && (index != LINGUAL_SULCUS2) &&
						(index != PARAHIPPOCAMPAL_GYRUS))
					continue ;
				dx = xw - v->x ; dy = yw - v->y ; dz = zw - v->z ; 
				dot = v->nx*dx + v->ny*dy + v->nz*dz ;
				dist = sqrt(dx*dx+dy*dy+dz*dz) ;
				if (dot < 0 && dist < MAX_DIST)
				{
					if (x == Gx && y == Gy && z == Gz)
						printf("voxel (%d, %d, %d): label %d (%s), parc %d, changing to wm...\n",
									 Gx, Gy, Gz, label, cma_label_to_name(label), index) ;
					changed++ ;
					MRIvox(mri, x, y, z) = right ? Right_Cerebral_White_Matter : Left_Cerebral_White_Matter ;
				}
      }
    }
  }
  total_changed = changed ; i = 0 ;
  
  /* look for stuff that is above the parahippocampal gyrus and labeled cortex, and
     change it to hippocampus */
  do
  {
    if (++i > 10)
      break ;
    changed = 0 ;
    for (x = 0 ; x < mri->width ; x++)
    {
      for (y = 0 ; y < mri->height ; y++)
      {
				for (z = 0 ; z < mri->depth ; z++)
				{
					if (x == Gx && y == Gy && z == Gz)
						DiagBreak() ;
					if (MRIvox(mri_below_para, x, y, z) > 0)
						continue ;
					if (MRIvox(mri_above_para, x, y, z) == 0)
						continue ;
					label = MRIvox(mri, x, y, z) ;
					/* only process cortical voxels */
					if ((right && ((label != Right_Cerebral_Cortex))) ||
							(!right && ((label != Left_Cerebral_Cortex))))
						continue ;
					MRIvoxelToSurfaceRAS(mri, x, y, z, &xw, &yw, &zw) ;
#if 0
					v = MHTfindClosestVertexInTable(mht, mris, xw, yw, zw) ;
					if (v == NULL)
						continue ;
					dist = sqrt(SQR(v->x-xw)+SQR(v->y-yw)+SQR(v->z-zw)) ;
					if (dist > MAX_DIST)
						continue ;   /* too far away - not reliable */
					index = CTABannotationToIndex(mris->ct, v->annotation) ;
#endif
					if ((MRIneighbors(mri, x, y, z, right ? Right_Hippocampus : Left_Hippocampus) == 0) &&
							(MRIneighbors(mri, x, y, z, right ? Right_Inf_Lat_Vent : Left_Inf_Lat_Vent) == 0))
						continue ;   /* no hippocampal or inf lat vent neighbors - ignore it */
					if (MRIvox(mri_above_inf_lat_vent, x, y, z) > 0)
						continue ;
					/*					if (index == MEDIAL_WALL)*/
					{
						changed++ ;
						if (MRIneighbors3x3(mri, x, y, z, right ? Right_Hippocampus : Left_Hippocampus) <
								MRIneighbors3x3(mri, x, y, z, right ? Right_Amygdala : Left_Amygdala))
							MRIvox(mri, x, y, z) = right ? Right_Amygdala : Left_Amygdala ;
						else
							MRIvox(mri, x, y, z) = right ? Right_Hippocampus : Left_Hippocampus ;
					}
				}
      }
    }
    printf("pass %d: changed %d...\n", i, changed) ;
    total_changed += changed ;
  } while (changed > 0) ;
  
  printf("%d voxels changed in medial temporal lobe...\n", total_changed) ;
  MHTfree(&mht) ; MRIfree(&mri_changed) ; MRIfree(&mri_above_para) ;
  MRIfree(&mri_aparc) ;
  MRIfree(&mri_above_inf_lat_vent) ; MRIfree(&mri_inside) ; MRIfree(&mri_below_para) ;
  return(NO_ERROR) ;
}
static int
edit_unknowns(MRI *mri_aseg, MRI *mri)
{
  int    x, y, z, label, right ;
  double mean_wm, mean_vent ;
  Real   val ;
  
  /*  find unknown voxels that border both wm and inf_lat_vent, and change them to one
      or the other */
  for (x = 0 ; x < mri->width ; x++)
  {
    for (y = 0 ; y < mri->height ; y++)
    {
      for (z = 0 ; z < mri->depth ; z++)
      {
	if (x == Gx && y == Gy && z == Gz)
	  DiagBreak() ;
	label = MRIvox(mri_aseg, x, y, z) ;
	/* only process cortical voxels */
	if (IS_UNKNOWN(label) == 0)
	  continue ;
	if ((MRIneighbors(mri_aseg, x, y, z, Left_Cerebral_White_Matter) > 0) &&
	    (MRIneighbors(mri_aseg, x, y, z, Left_Inf_Lat_Vent) > 0))
	{
	  right = 0 ;
	  mean_vent = MRIlabelMean(mri, mri_aseg, x, y, z, 1, Left_Inf_Lat_Vent) ;
	  mean_wm = MRIlabelMean(mri, mri_aseg, x, y, z, 1, Left_Cerebral_White_Matter) ;
	}
	else if ((MRIneighbors(mri_aseg, x, y, z, Right_Cerebral_White_Matter) > 0) &&
		 (MRIneighbors(mri_aseg, x, y, z, Right_Inf_Lat_Vent) > 0))
	{
	  mean_vent = MRIlabelMean(mri, mri_aseg, x, y, z, 1, Right_Inf_Lat_Vent) ;
	  mean_wm = MRIlabelMean(mri, mri_aseg, x, y, z, 1, Right_Cerebral_White_Matter) ;
	  right = 1 ;
	}
	else
	  continue ; /* must nbr both vent and wm, otherwise don't process it */
	
	MRIsampleVolume(mri, x, y, z, &val) ;
	if (fabs(val-mean_wm) < fabs(val-mean_vent))
	{
	  MRIvox(mri_aseg, x, y, z) = right ? Right_Cerebral_White_Matter : Left_Cerebral_White_Matter ;
	}
	else
	{
	  MRIvox(mri_aseg, x, y, z) = right ? Right_Inf_Lat_Vent : Left_Inf_Lat_Vent ;
	}
      }
    }
  }
  return(NO_ERROR) ;
}
#endif

#if 0
static int
edit_hippocampus(MRI *mri)
{
  int   x, y, z, right, changed, total = 0, yi, yk, found, label ;
  
  do
  {
    changed =  0 ;
    for (x = 0 ; x < mri->width ; x++)
    {
      for (y = 0 ; y < mri->height ; y++)
      {
	for (z = 0 ; z < mri->depth ; z++)
	{
	  if (x == Gx && y == Gy && z == Gz)
	    DiagBreak() ;
	  label = MRIvox(mri, x, y, z) ;
	  if (IS_CORTEX(label) == 0)
	    continue ;
	  right = (label == Right_Cerebral_Cortex) ;
	  yi = mri->yi[y-1] ;  /* voxel immediately superior */
	  if (IS_HIPPO(MRIvox(mri, x, yi, z)) == 0)
	    continue ;
	  
	  /* check for wm within 3 mm inferior */
	  for (found = 0, yk = 1 ; yk <= 3 ; yk++)
	  {
	    yi = mri->yi[y+yk] ;  /* inferior voxel */
	    if (IS_WM(MRIvox(mri, x, yi, z)) != 0)
	    {
	      found = 1 ;
	      break ;
	    }
	  }
	  if (!found)
	    continue ;
	  
	  if (x == Gx && y == Gy && z == Gz)
	    printf("changing voxel (%d, %d, %d) from %s to ",Gx, Gy, Gz, cma_label_to_name(label)) ;
	  changed++ ;
	  MRIvox(mri, x, y, z) = right ? Right_Hippocampus : Left_Hippocampus ;
	  if (x == Gx && y == Gy && z == Gz)
	    printf("%s\n", cma_label_to_name(MRIvox(mri, x, y, z))) ;
	}
      }
    }
    total += changed ;
  } while (changed > 0) ;
  
  printf("%d cortex voxels changed to hippocampus...\n", total) ;
  return(NO_ERROR)  ;
}
#endif

double
MRIlabelMean(MRI *mri, MRI  *mri_labels, int x, int y, int z, int whalf, int label)
{
  int    xi, yi, zi, xk, yk, zk, nvox ;
  double mean ;
  Real   val ;
  
  mean = 0.0 ; nvox = 0 ;
  for (xk = -whalf ; xk <= whalf ; xk++)
  {
    xi = mri->xi[x+xk] ;
    for (yk = -whalf ; yk <= whalf ; yk++)
    {
      yi = mri->yi[y+yk] ;
      for (zk = -whalf ; zk <= whalf ; zk++)
      {
	zi = mri->zi[z+zk] ;
	if (MRIvox(mri_labels, xi, yi, zi) != label)
	  continue ;
	MRIsampleVolume(mri, xi, yi, zi, &val) ;
	mean += val ; nvox++ ;
      }
    }
  }
  if (nvox > 0)
    mean /= (double)nvox ;
  return(mean) ;
}
#if 0
static int
edit_calcarine(MRI *mri, MRI_SURFACE *mris, int right)
{
  MRI     *mri_calc ;
  int     vno, changed, label, index, x, y, z ;
  VERTEX  *v ;
  double  d ;
  Real    xv, yv, zv, xs, ys, zs ;

  mri_calc = MRIclone(mri, NULL) ;
  
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    index = CTABannotationToIndex(mris->ct, v->annotation) ;
    if (index != CALCARINE_SULCUS)
      continue ;
    for (d = 1  ; d <= 4  ; d += 0.5)
    {
      xs = v->x + v->nx*d ; ys = v->y + v->ny*d ; zs = v->z + v->nz*d ;
      // MRIworldToVoxel(mri, xs, ys, zs, &xv, &yv, &zv) ;      
      MRIsurfaceRASToVoxel(mri, xs, ys, zs, &xv, &yv, &zv) ;
      x = nint(xv) ; y = nint(yv) ; z = nint(zv) ;
      if (x == Gx && y == Gy && z == Gz)
	DiagBreak() ;
      
      MRIvox(mri_calc, x, y, z) = 128 ;
    }
  }
  
  MRIclose(mri_calc, mri_calc) ;
  
  for (changed = x = 0 ; x < mri->width ; x++)
  {
    for (y =  0 ; y < mri->height ; y++)
    {
      for (z = 0 ; z < mri->depth ; z++)
      {
	if (x == Gx && y == Gy && z == Gz)
	  DiagBreak() ;
	label = MRIvox(mri, x, y, z) ;
	if (((label == Left_Lateral_Ventricle || label == Right_Lateral_Ventricle)) &&
	    (MRIvox(mri_calc, x, y, z) > 0))
	{
	  changed++ ;
	  MRIvox(mri, x, y, z) = Unknown ;
	  if (x == Gx && y == Gy && z == Gz)
	    printf("label %s at (%d, %d, %d) changed to %s\n",
		   cma_label_to_name(label), x, y, z, cma_label_to_name(MRIvox(mri, x, y, z))) ;
	}
      }
    }
  }
  
  printf("%d ventricular voxels changed to unknown\n", changed) ;
  MRIfree(&mri_calc) ;
  return(NO_ERROR) ;
}

#endif
