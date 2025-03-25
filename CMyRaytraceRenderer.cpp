#include "pch.h"
#include "CMyRaytraceRenderer.h"
#include "graphics/GrTexture.h"

void CMyRaytraceRenderer::SetWindow(CWnd* p_window)
{
    m_window = p_window;
}

bool CMyRaytraceRenderer::RendererStart()
{
	m_intersection.Initialize();

	m_mstack.clear();


	// We have to do all of the matrix work ourselves.
	// Set up the matrix stack.
	CGrTransform t;
	t.SetLookAt(Eye().X(), Eye().Y(), Eye().Z(),
		Center().X(), Center().Y(), Center().Z(),
		Up().X(), Up().Y(), Up().Z());

	m_mstack.push_back(t);

	m_material = NULL;

	return true;
}

void CMyRaytraceRenderer::RendererMaterial(CGrMaterial* p_material)
{
	m_material = p_material;
}

void CMyRaytraceRenderer::RendererPushMatrix()
{
	m_mstack.push_back(m_mstack.back());
}

void CMyRaytraceRenderer::RendererPopMatrix()
{
	m_mstack.pop_back();
}

void CMyRaytraceRenderer::RendererRotate(double a, double x, double y, double z)
{
	CGrTransform r;
	r.SetRotate(a, CGrPoint(x, y, z));
	m_mstack.back() *= r;
}

void CMyRaytraceRenderer::RendererTranslate(double x, double y, double z)
{
	CGrTransform r;
	r.SetTranslate(x, y, z);
	m_mstack.back() *= r;
}

// Checks if shadow is there or not
bool CMyRaytraceRenderer::ShadowFeeler(CGrPoint intersect, Light light, const CRayIntersection::Object* nearest, double t)
{
	CRay lightRay(intersect, Normalize3(m_mstack.back() * light.m_pos)); // Initialize the renderer and the matrix stack (tutorial 9th)
	const CRayIntersection::Object* lightNearest;

	if (m_intersection.Intersect(lightRay, 1e20, nearest, lightNearest, t, intersect)) return true;
	else return false;
}

CGrPoint CMyRaytraceRenderer::RayColor(CRay ray)
{
	double t;                                   // Will be distance to intersection
	CGrPoint intersect;                         // Will by x,y,z location of intersection
	const CRayIntersection::Object* nearest;    // Pointer to intersecting object
	CGrPoint color = { 0., 0., 0. };

	if (m_intersection.Intersect(ray, 1e20, NULL, nearest, t, intersect))
	{
		// We hit something...
		// Determine information about the intersection
		CGrPoint N;
		CGrMaterial* material;
		CGrTexture* texture;
		CGrPoint texcoord;

		m_intersection.IntersectInfo(ray, nearest, t,
			N, material, texture, texcoord);

		if (material != NULL)
		{
			int lightCnt = this->LightCnt();

			float lightInt = 0.7; // TO DO

			Light light = this->GetLight(0);

			CGrPoint ambient = { light.m_ambient[0] * material->Ambient(0),
								 light.m_ambient[1] * material->Ambient(1),
								 light.m_ambient[2] * material->Ambient(2) };

			if (texture != nullptr)
			{
				double tex = double(texture->Row(texcoord.Y() * texture->Height())[3 * int((texcoord.X() * texture->Width())) + 0]) / 255.0;

				ambient = { ambient[0] * tex, ambient[1] * tex, ambient[2] * tex };
			}

			color += ambient;

			for (int l = 0; l < lightCnt; l++)
			{
				if (ShadowFeeler(intersect, light, nearest, t) == false)
				{
					light = this->GetLight(l);
					CGrPoint s = Normalize3(light.m_pos); // Light Dir
					CGrPoint v = Normalize3(-intersect); // View Dir
					CGrPoint n = Normalize3(N); // Normal Vector
					CGrPoint h = Normalize3(v + s); // Halfway Vector

					float diffuse_x = light.m_diffuse[0] * material->Diffuse(0) * lightInt * max(0.f, Dot3(n, s));
					float diffuse_y = light.m_diffuse[1] * material->Diffuse(1) * lightInt * max(0.f, Dot3(n, s));
					float diffuse_z = light.m_diffuse[2] * material->Diffuse(2) * lightInt * max(0.f, Dot3(n, s));

					CGrPoint diffuse = { diffuse_x , diffuse_y , diffuse_z };

					if (texture != nullptr)
					{
						double tex = double(texture->Row(texcoord.Y() * texture->Height())[3 * int((texcoord.X() * texture->Width())) + 0]) / 255.0;

						diffuse = { tex * diffuse_x , tex * diffuse_y , tex * diffuse_z };
					}

					float spec_x = light.m_specular[0] * material->Specular(0) * pow(max(0.f, Dot3(n, h)), material->Shininess());
					float spec_y = light.m_specular[1] * material->Specular(1) * pow(max(0.f, Dot3(n, h)), material->Shininess());
					float spec_z = light.m_specular[2] * material->Specular(2) * pow(max(0.f, Dot3(n, h)), material->Shininess());

					CGrPoint specular = { spec_x , spec_y , spec_z };

					color += diffuse + specular;
				}
			}
		}
	}

	return color;
}

//
// Name : CMyRaytraceRenderer::RendererEndPolygon()
// Description : End definition of a polygon. The superclass has
// already collected the polygon information
//

void CMyRaytraceRenderer::RendererEndPolygon()
{
    const std::list<CGrPoint>& vertices = PolyVertices();
    const std::list<CGrPoint>& normals = PolyNormals();
    const std::list<CGrPoint>& tvertices = PolyTexVertices();

    // Allocate a new polygon in the ray intersection system
    m_intersection.PolygonBegin();
    m_intersection.Material(m_material);

    if (PolyTexture())
    {
        m_intersection.Texture(PolyTexture());
    }

    std::list<CGrPoint>::const_iterator normal = normals.begin();
    std::list<CGrPoint>::const_iterator tvertex = tvertices.begin();

    for (std::list<CGrPoint>::const_iterator i = vertices.begin(); i != vertices.end(); i++)
    {
        if (normal != normals.end())
        {
            m_intersection.Normal(m_mstack.back() * *normal);
            normal++;
        }

        if (tvertex != tvertices.end())
        {
            m_intersection.TexVertex(*tvertex);
            tvertex++;
        }

        m_intersection.Vertex(m_mstack.back() * *i);
    }

    m_intersection.PolygonEnd();
}

bool CMyRaytraceRenderer::RendererEnd()
{
	m_intersection.LoadingComplete();

	double ymin = -tan(ProjectionAngle() / 2 * GR_DTOR);
	double yhit = -ymin * 2;

	double xmin = ymin * ProjectionAspect();
	double xwid = -xmin * 2;

	for (int r = 0; r < m_rayimageheight; r++)
	{
		for (int c = 0; c < m_rayimagewidth; c++)
		{
			double colorTotal[3] = { 0, 0, 0 };

			double x = xmin + (c + 0.5) / m_rayimagewidth * xwid;
			double y = ymin + (r + 0.5) / m_rayimageheight * yhit;


			// Construct a Ray
			CRay ray(CGrPoint(0, 0, 0), Normalize3(CGrPoint(x, y, -1, 0)));

			CGrPoint color = RayColor(ray);

			m_rayimage[r][c * 3] = BYTE(color[0] * 255);
			m_rayimage[r][c * 3 + 1] = BYTE(color[1] * 255);
			m_rayimage[r][c * 3 + 2] = BYTE(color[2] * 255);

		}
		if ((r % 50) == 0)
		{
			m_window->Invalidate();
			MSG msg;
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				DispatchMessage(&msg);
		}
	}


	return true;
}
